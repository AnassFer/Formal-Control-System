#include "robot_simulation.hpp"

#include "symctrl/factories.hpp"
#include "symctrl/synthesis/product_model.hpp"
#include "symctrl/synthesis/reachability_controller.hpp"

#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>
#include <utility>

namespace robot_app {
namespace {

symctrl::Vector linspace(double lo, double hi, std::size_t count) {
    if (count == 0) {
        throw std::invalid_argument("linspace: count must be positive");
    }
    symctrl::Vector values(count);
    if (count == 1) {
        values[0] = lo;
        return values;
    }
    const double step = (hi - lo) / static_cast<double>(count - 1);
    for (std::size_t i = 0; i < count; ++i) {
        values[i] = lo + static_cast<double>(i) * step;
    }
    values.back() = hi;
    return values;
}

void report(const ProgressCallback& cb, int step, int total, const std::string& msg) {
    if (cb) {
        cb(step, total, msg);
    }
}

} // namespace

RobotConfig makePaperDefaultConfig(bool fullPaperGrid) {
    RobotConfig cfg;
    if (fullPaperGrid) {
        cfg.nx1 = 100;
        cfg.nx2 = 100;
        cfg.nx3 = 30;
    } else {
        cfg.nx1 = 20;
        cfg.nx2 = 20;
        cfg.nx3 = 12;
    }

    // Tableau 2 du document S7467
    cfg.regions = {
        {"R1", 4.0, 5.0, 8.5, 9.5, {0.95f, 0.85f, 0.20f, 0.45f}},
        {"R2", 8.5, 9.5, 2.0, 3.0, {0.30f, 0.70f, 0.95f, 0.45f}},
        {"R3", 2.0, 3.0, 0.5, 1.5, {0.45f, 0.85f, 0.35f, 0.45f}},
        {"R4", 3.0, 7.0, 3.0, 7.0, {0.95f, 0.45f, 0.30f, 0.50f}}
    };

    cfg.mission.visitFirst = {"R1", "R2"};
    cfg.mission.visitFirstExclusive = true;
    cfg.mission.thenGoTo = {"R3"};
    cfg.mission.avoid = {"R4"};
    return cfg;
}

SynthesisOutput runRobotSynthesis(const RobotConfig& config,
                                  const ProgressCallback& onProgress) {
    using namespace symctrl;
    using namespace symctrl::part2;

    const auto totalStart = std::chrono::steady_clock::now();

    report(onProgress, 1, 5, "Construction de la grille et des commandes...");
    GridSpec gridSpec;
    gridSpec.domain = Box{
        {config.x1Min, config.x2Min, config.x3Min},
        {config.x1Max, config.x2Max, config.x3Max}
    };
    gridSpec.cells_per_dimension = {config.nx1, config.nx2, config.nx3};

    auto grid = makeUniformGrid(gridSpec);
    auto inputs = makeCartesianInputs({
        linspace(config.u1Min, config.u1Max, config.nu1),
        linspace(config.u2Min, config.u2Max, config.nu2)
    });

    const Box disturbance{
        {-config.w1Max, -config.w2Max, -config.w3Max},
        { config.w1Max,  config.w2Max,  config.w3Max}
    };

    auto dynamics = std::make_shared<UnicycleDynamics>(config.tau);
    ReachabilityOptions reachOpts{};
    reachOpts.require_validated = false;
    auto reachability = makeReachability(dynamics, ReachabilityMethod::BoundedDerivative, reachOpts);

    BuildOptions buildOpts{};
    buildOpts.max_state_input_pairs = 50000000ULL;
    buildOpts.max_stored_edges = 500000000ULL;
    buildOpts.max_estimated_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;

    report(onProgress, 2, 5, "Construction du modele symbolique...");
    auto model = buildSymbolicModel(*grid, *inputs, *reachability, disturbance, buildOpts);

    report(onProgress, 3, 5, "Compilation de l'automate et du modele produit...");
    Automaton automaton = compileMissionToAutomaton(config.regions, config.mission);
    StateLabels labels = buildRegionLabels(*grid, config.regions);
    auto productModel = buildProductModel(*model, automaton, labels, buildOpts);
    StateSet target = buildProductTarget(grid->internalStateCount(), automaton);

    report(onProgress, 4, 5, "Synthese du controleur d'atteignabilite...");
    ReachabilityResult reachResult = computeReachabilityDomain(*productModel, target);
    ReachabilityController rawController = buildReachabilityController(*productModel, reachResult);

    report(onProgress, 5, 5, "Assemblage de la politique symbolique...");
    StateSet winningInit = computeWinningInitialStates(
        grid->internalStateCount(), automaton, labels, reachResult.domain);

    auto policy = std::make_shared<AutomatonPolicy>(
        grid->internalStateCount(), inputs->inputCount(),
        automaton, labels, std::move(rawController));

    SynthesisOutput out;
    out.grid = std::move(grid);
    out.inputs = std::move(inputs);
    out.dynamics = std::move(dynamics);
    out.automaton = std::move(automaton);
    out.labels = std::move(labels);
    out.winningInitialStates = std::move(winningInit);
    out.policy = std::move(policy);
    out.storedEdges = model->statistics().stored_edges;
    out.abstractionSeconds = model->statistics().abstraction_seconds;
    out.totalSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - totalStart).count();
    return out;
}

Trajectory simulateClosedLoop(const SynthesisOutput& synth,
                              const RobotConfig& config,
                              const symctrl::Vector& x0,
                              std::uint32_t seed,
                              std::size_t maxSteps) {
    Trajectory result;
    if (!synth.grid || !synth.inputs || !synth.dynamics || !synth.policy) {
        result.statusMessage = "Controleur non initialise.";
        return result;
    }

    const symctrl::StateId startId = synth.grid->locate(x0);
    if (startId == symctrl::kOutside) {
        result.statusMessage = "Etat initial hors du domaine.";
        return result;
    }
    if (synth.winningInitialStates.count(startId) == 0) {
        result.statusMessage = "Etat initial hors du domaine gagnant Q0.";
        return result;
    }

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> distW1(-config.w1Max, config.w1Max);
    std::uniform_real_distribution<double> distW2(-config.w2Max, config.w2Max);
    std::uniform_real_distribution<double> distW3(-config.w3Max, config.w3Max);

    symctrl::Vector x = x0;
    symctrl::PolicyMemory memory = synth.policy->initialMemory();

    for (std::size_t t = 0; t < maxSteps; ++t) {
        const symctrl::StateId state = synth.grid->locate(x);
        if (state == symctrl::kOutside) {
            result.statusMessage = "Sortie du domaine a t=" + std::to_string(t);
            return result;
        }
        if (synth.policy->isFinished(memory, state)) {
            const auto label = synth.labels.at(state);
            const auto finalMem = symctrl::part2::nextAutomatonState(
                synth.automaton,
                static_cast<symctrl::part2::AutomatonStateId>(memory.value),
                label);
            result.points.push_back({t, x[0], x[1], x[2], 0.0, 0.0,
                                     static_cast<std::uint64_t>(finalMem)});
            result.success = true;
            result.statusMessage = "Mission reussie en " + std::to_string(t) + " pas.";
            return result;
        }

        const auto decision = symctrl::refineDecision(*synth.grid, *synth.inputs, *synth.policy, x, memory);
        result.points.push_back({
            t, x[0], x[1], x[2],
            decision.physical_input[0], decision.physical_input[1],
            decision.next_memory.value
        });

        const symctrl::Vector w{distW1(rng), distW2(rng), distW3(rng)};
        x = synth.dynamics->step(x, decision.physical_input, w);
        memory = decision.next_memory;
    }

    result.statusMessage = "Horizon maximal atteint (" + std::to_string(maxSteps) + " pas).";
    return result;
}

void exportTrajectoryCsv(const Trajectory& trajectory, const std::string& filePath) {
    std::ofstream out(filePath);
    if (!out) {
        throw std::runtime_error("Impossible d'ouvrir le fichier CSV : " + filePath);
    }
    out << "t,x1,x2,x3,u1,u2,mem\n";
    for (const auto& p : trajectory.points) {
        out << p.step << ',' << p.x1 << ',' << p.x2 << ',' << p.x3
            << ',' << p.u1 << ',' << p.u2 << ',' << p.memory << '\n';
    }
}

} // namespace robot_app