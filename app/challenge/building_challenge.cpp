#include "building_challenge.hpp"

#include "symctrl/factories.hpp"
#include "symctrl/model_bundle.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace challenge_app {
namespace {

void report(const ProgressCallback& cb, int step, int total, const std::string& msg) {
    if (cb) {
        cb(step, total, msg);
    }
}

bool tryPowUint64(std::uint64_t base, std::size_t exp, std::uint64_t& out) {
    std::uint64_t result = 1;
    for (std::size_t i = 0; i < exp; ++i) {
        if (base != 0 && result > std::numeric_limits<std::uint64_t>::max() / base) {
            return false;
        }
        result *= base;
    }
    out = result;
    return true;
}

bool allRoomsInComfortZone(const symctrl::Vector& temperatures) {
    for (double temp : temperatures) {
        if (!std::isfinite(temp) || temp < kTempMin || temp > kTempMax) {
            return false;
        }
    }
    return true;
}

double inputEffort(const symctrl::Vector& u) {
    return std::accumulate(u.begin(), u.end(), 0.0);
}

} // namespace

SafetyTablePolicy::SafetyTablePolicy(const symctrl::part2::SafetyController& controller,
                                     const symctrl::IInputCatalog& inputs,
                                     ActionStrategy strategy)
    : controller_(controller), inputs_(inputs), strategy_(strategy) {}

symctrl::PolicyDecision SafetyTablePolicy::decide(symctrl::PolicyMemory previous_memory,
                                                  symctrl::StateId current_state) const {
    const auto it = controller_.find(current_state);
    if (it == controller_.end() || it->second.empty()) {
        throw std::out_of_range("SafetyTablePolicy: no safe input defined for current state");
    }

    const auto& allowed = it->second;
    symctrl::InputId chosen = allowed.front();

    if (strategy_ == ActionStrategy::MinEnergy) {
        double bestEffort = inputEffort(inputs_.value(chosen));
        for (std::size_t i = 1; i < allowed.size(); ++i) {
            const double effort = inputEffort(inputs_.value(allowed[i]));
            if (effort < bestEffort) {
                bestEffort = effort;
                chosen = allowed[i];
            }
        }
    } else if (strategy_ == ActionStrategy::MaxHeating) {
        double bestEffort = inputEffort(inputs_.value(chosen));
        for (std::size_t i = 1; i < allowed.size(); ++i) {
            const double effort = inputEffort(inputs_.value(allowed[i]));
            if (effort > bestEffort) {
                bestEffort = effort;
                chosen = allowed[i];
            }
        }
    }

    // Safety controllers are static (memoryless, Paper §2.1.2 / Corollary 1).
    return symctrl::PolicyDecision{previous_memory, chosen};
}

ScaleEstimate estimateChallengeScale(std::size_t rooms) {
    ScaleEstimate est;
    est.rooms = rooms;
    if (rooms == 0) {
        return est;
    }

    const double nd = static_cast<double>(rooms);
    est.statesApprox = std::pow(static_cast<double>(kCellsPerDimension), nd);
    est.inputsApprox = std::pow(static_cast<double>(kHeaterInputs.size()), nd);
    est.pairsApprox = (est.statesApprox + 1.0) * est.inputsApprox;

    // Each coordinate's one-step monotone image has width in [0.4505, 0.4775] < eta=0.5,
    // intersecting ~1.90 cells/dimension empirically (184,832 edges for N=3 => 1.899^3/pair).
    est.estimatedEdgesApprox = est.statesApprox * est.inputsApprox * std::pow(1.90, nd);

    // ExplicitTransitionSystem memory:
    //   - Row headers: sizeof(SuccessorSet) per state-input pair
    //   - Edge IDs: sizeof(StateId) per stored edge
    //   - Heap allocation overhead per non-empty vector (~24 B average)
    const double headerBytes = est.pairsApprox * static_cast<double>(sizeof(symctrl::SuccessorSet));
    const double edgeBytes = est.estimatedEdgesApprox * static_cast<double>(sizeof(symctrl::StateId));
    const double allocatorOverheadBytes = est.statesApprox * est.inputsApprox * 24.0;
    est.estimatedRamMiB = (headerBytes + edgeBytes + allocatorOverheadBytes) / (1024.0 * 1024.0);

    est.statesFitUint64 = tryPowUint64(kCellsPerDimension, rooms, est.symbolicStates);
    est.inputsFitUint64 = tryPowUint64(kHeaterInputs.size(), rooms, est.symbolicInputs);
    if (est.statesFitUint64 && est.inputsFitUint64) {
        if (est.symbolicStates < std::numeric_limits<std::uint64_t>::max()) {
            const std::uint64_t allStates = est.symbolicStates + 1;
            if (est.symbolicInputs == 0
                || allStates <= std::numeric_limits<std::uint64_t>::max() / est.symbolicInputs) {
                est.stateInputPairs = allStates * est.symbolicInputs;
                est.pairsFitUint64 = true;
            }
        }
    }

    return est;
}

MemorySnapshot queryProcessMemory() {
    MemorySnapshot snap;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) != 0) {
        snap.currentRssMiB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        snap.peakRssMiB = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
        snap.available = true;
    }
#elif defined(__APPLE__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        snap.peakRssMiB = static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
        snap.available = true;
    }
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        snap.currentRssMiB = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
        snap.available = true;
    }
#elif defined(__linux__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        snap.peakRssMiB = static_cast<double>(usage.ru_maxrss) / 1024.0;
        snap.available = true;
    }
    std::ifstream statm("/proc/self/statm");
    std::uint64_t sizePages = 0;
    std::uint64_t residentPages = 0;
    if (statm >> sizePages >> residentPages) {
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            snap.currentRssMiB = (static_cast<double>(residentPages) * static_cast<double>(pageSize))
                                 / (1024.0 * 1024.0);
            snap.available = true;
        }
    }
#endif
    return snap;
}

ChallengeSynthesisOutput runBuildingChallenge(const ChallengeConfig& config,
                                              const ProgressCallback& onProgress) {
    using namespace symctrl;
    using namespace symctrl::part2;

    if (config.rooms < 3) {
        throw std::invalid_argument("Circular building challenge requires N >= 3 rooms.");
    }

    const ScaleEstimate estimate = estimateChallengeScale(config.rooms);
    const std::uint64_t budgetMiB = config.memoryBudgetMiB == 0 ? 1ULL : config.memoryBudgetMiB;
    if (!estimate.pairsFitUint64) {
        throw std::length_error("N=" + std::to_string(config.rooms)
                                + " overflows 64-bit explicit state-input pair counts.");
    }
    if (estimate.estimatedRamMiB > static_cast<double>(budgetMiB) * 1.15) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "N=" << config.rooms << " requires approximately " << estimate.estimatedRamMiB
            << " MiB (" << (estimate.estimatedRamMiB / 1024.0)
            << " GiB) in explicit graph form, exceeding the configured budget of "
            << budgetMiB << " MiB.";
        throw std::length_error(oss.str());
    }

    report(onProgress, 1, 5, "Building N-dimensional grid, input catalog, and ring dynamics...");

    GridSpec spec;
    spec.domain = Box{
        Vector(config.rooms, kTempMin),
        Vector(config.rooms, kTempMax)
    };
    spec.cells_per_dimension = GridIndex(config.rooms, kCellsPerDimension);
    spec.periodic = std::vector<bool>(config.rooms, false);

    auto grid = makeUniformGrid(spec);

    std::vector<Vector> inputAxes(
        config.rooms,
        Vector(kHeaterInputs.begin(), kHeaterInputs.end()));
    auto inputs = makeCartesianInputs(std::move(inputAxes));

    auto dynamics = makeRingBuildingDynamics(config.rooms);
    ReachabilityOptions reachOpts{};
    reachOpts.require_validated = false;
    auto reachability = makeReachability(dynamics, config.reachabilityMethod, reachOpts);

    report(onProgress, 2, 5, "Computing symbolic abstraction (T_abs)...");

    BuildOptions buildOpts;
    buildOpts.max_state_input_pairs = std::numeric_limits<std::uint64_t>::max();
    buildOpts.max_stored_edges = std::numeric_limits<std::uint64_t>::max();
    buildOpts.max_estimated_bytes = checkedMultiply(budgetMiB, 1024ULL * 1024ULL);

    // Circular building has zero external disturbance inputs (q = 0).
    const Box emptyDisturbance{{}, {}};
    auto model = buildSymbolicModel(*grid, *inputs, *reachability, emptyDisturbance, buildOpts);

    report(onProgress, 3, 5, "Computing maximal safety domain R* (Algorithm 1)...");

    const auto synthStart = std::chrono::steady_clock::now();
    const StateId M = grid->internalStateCount();
    StateSet safeStates;
    for (StateId s = 1; s <= M; ++s) {
        safeStates.insert(safeStates.end(), s);
    }

    StateSet safetyDomain = computeSafetyDomain(*model, safeStates);

    report(onProgress, 4, 5, "Extracting maximal safety controller H (Theorem 2)...");
    SafetyController controller = buildSafetyController(*model, safetyDomain);

    const double synthesisSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - synthStart).count();

    report(onProgress, 5, 5, "Collecting competition metrics...");
    const MemorySnapshot mem = queryProcessMemory();
    const BuildStatistics& stats = model->statistics();

    ChallengeMetrics metrics;
    metrics.rooms = config.rooms;
    metrics.internalStates = M;
    metrics.inputCount = inputs->inputCount();
    metrics.processedPairs = stats.processed_pairs;
    metrics.storedEdges = stats.stored_edges;
    metrics.controllableStates = static_cast<std::uint64_t>(safetyDomain.size());
    metrics.controllablePercent = M > 0
        ? (100.0 * static_cast<double>(metrics.controllableStates) / static_cast<double>(M))
        : 0.0;
    metrics.abstractionSeconds = stats.abstraction_seconds;
    metrics.synthesisSeconds = synthesisSeconds;
    metrics.totalSeconds = metrics.abstractionSeconds + metrics.synthesisSeconds;
    metrics.peakMemoryMiB = mem.peakRssMiB;
    metrics.currentMemoryMiB = mem.currentRssMiB;
    metrics.memoryMeasured = mem.available;
    metrics.numericGuarantee = model->numericGuarantee();
    metrics.reachabilityMethod = config.reachabilityMethod;

    ChallengeSynthesisOutput output;
    output.grid = std::move(grid);
    output.inputs = std::move(inputs);
    output.dynamics = std::move(dynamics);
    output.model = std::move(model);
    output.safetyDomain = std::move(safetyDomain);
    output.controller = std::move(controller);
    output.metrics = metrics;
    return output;
}

BuildingTrajectory simulateBuildingClosedLoop(const ChallengeSynthesisOutput& synth,
                                              const symctrl::Vector& initialTemperatures,
                                              std::size_t horizon,
                                              ActionStrategy strategy) {
    BuildingTrajectory traj;
    if (!synth.grid || !synth.inputs || !synth.dynamics) {
        traj.statusMessage = "Controller is not initialized.";
        return traj;
    }

    const std::size_t n = synth.grid->stateDimension();
    if (initialTemperatures.size() != n) {
        traj.statusMessage = "Initial temperature vector dimension does not match N.";
        return traj;
    }

    const symctrl::StateId startState = synth.grid->locate(initialTemperatures);
    if (startState == symctrl::kOutside) {
        traj.statusMessage = "Initial state T(0) is outside [17, 22]^N.";
        return traj;
    }
    if (synth.safetyDomain.count(startState) == 0) {
        traj.statusMessage = "Initial cell is outside the winning safety domain R*.";
        return traj;
    }

    SafetyTablePolicy policy(synth.controller, *synth.inputs, strategy);
    symctrl::Vector x = initialTemperatures;
    symctrl::PolicyMemory memory{};
    bool everyStepSafe = true;

    traj.steps.reserve(horizon + 1);
    for (std::size_t t = 0; t < horizon; ++t) {
        const bool safeNow = allRoomsInComfortZone(x);
        everyStepSafe = everyStepSafe && safeNow;

        const symctrl::StateId curState = synth.grid->locate(x);
        if (curState == symctrl::kOutside) {
            traj.steps.push_back({t, curState, 0, false, false, x, symctrl::Vector(n, 0.0)});
            traj.allStepsSafe = false;
            traj.statusMessage = "Safety violated (left [17, 22]^N) at step t=" + std::to_string(t) + ".";
            return traj;
        }

        const symctrl::ConcreteDecision decision =
            symctrl::refineDecision(*synth.grid, *synth.inputs, policy, x, memory);

        traj.steps.push_back({
            t,
            decision.state,
            decision.input,
            true,
            safeNow,
            x,
            decision.physical_input
        });

        x = synth.dynamics->step(x, decision.physical_input, {});
        memory = decision.next_memory;
    }

    const bool finalSafe = allRoomsInComfortZone(x);
    everyStepSafe = everyStepSafe && finalSafe;
    const symctrl::StateId finalState = synth.grid->locate(x);
    traj.steps.push_back({horizon, finalState, 0, false, finalSafe, x, symctrl::Vector(n, 0.0)});

    traj.allStepsSafe = everyStepSafe;
    traj.statusMessage = everyStepSafe
        ? ("Safe for all " + std::to_string(horizon) + " steps in [17, 22]^" + std::to_string(n) + ".")
        : "Safety bound violated during closed-loop simulation.";
    return traj;
}

void exportBuildingTrajectoryCsv(const BuildingTrajectory& trajectory,
                                 const std::filesystem::path& filePath) {
    if (trajectory.steps.empty()) {
        throw std::invalid_argument("Cannot export empty trajectory.");
    }
    const std::size_t n = trajectory.steps.front().temperatures.size();
    std::ofstream out(filePath);
    if (!out) {
        throw std::runtime_error("Failed to open CSV file: " + filePath.string());
    }
    out << "t,state_id,input_id,safe";
    for (std::size_t i = 0; i < n; ++i) {
        out << ",T_" << (i + 1);
    }
    for (std::size_t i = 0; i < n; ++i) {
        out << ",u_" << (i + 1);
    }
    out << '\n';

    for (const auto& step : trajectory.steps) {
        out << step.step << ',' << step.stateId << ',' << step.inputId << ',' << (step.safe ? 1 : 0);
        for (double temp : step.temperatures) {
            out << ',' << temp;
        }
        for (double u : step.heaterInputs) {
            out << ',' << u;
        }
        out << '\n';
    }
}

void exportChallengeModelPackage(const ChallengeSynthesisOutput& synth,
                                 const std::filesystem::path& directory) {
    if (!synth.grid || !synth.inputs || !synth.model) {
        throw std::invalid_argument("No synthesized model available to export.");
    }

    symctrl::ModelMetadata meta;
    meta.model_id = "ring_building_n" + std::to_string(synth.metrics.rooms);
    meta.source = "Saoud Project - Symbolic Control (Circular Building Challenge)";
    meta.provenance = symctrl::Provenance::GeneratedPart1;
    meta.model_parameters = {
        {"N", static_cast<double>(synth.metrics.rooms)},
        {"alpha", kAlpha},
        {"beta", kBeta},
        {"gamma", kGamma},
        {"Te", kOutsideTemp},
        {"Th", kHeaterTemp}
    };
    meta.state_dimension = synth.grid->stateDimension();
    meta.input_dimension = synth.inputs->inputDimension();
    meta.disturbance_dimension = 0;
    meta.internal_states = synth.grid->internalStateCount();
    meta.input_count = synth.inputs->inputCount();
    meta.grid = synth.grid->spec();
    meta.disturbance = symctrl::Box{{}, {}};
    meta.numeric_guarantee = synth.model->numericGuarantee();

    const symctrl::ModelBundle bundle = symctrl::assembleModelBundle(
        synth.grid, synth.inputs, synth.model, std::move(meta));

    symctrl::ExportOptions exportOpts{};
    exportOpts.max_transition_rows = std::max<std::uint64_t>(
        exportOpts.max_transition_rows,
        symctrl::checkedAdd(synth.metrics.storedEdges, synth.metrics.inputCount));
    symctrl::exportModelPackage(bundle, directory, exportOpts);
}

} // namespace challenge_app
