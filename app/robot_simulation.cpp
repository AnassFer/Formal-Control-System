#include "robot_simulation.hpp"

#include "symctrl/factories.hpp"
#include "symctrl/synthesis/product_model.hpp"
#include "symctrl/synthesis/reachability_controller.hpp"

#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>
#include <utility>
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>

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

struct RelativeTransition {
    bool feasible = false;
    int minI1 = 0, maxI1 = -1;
    int minI2 = 0, maxI2 = -1;
    int d1Lo = 0, d1Hi = -1;
    int d2Lo = 0, d2Hi = -1;
    std::vector<std::uint32_t> nextI3;
};

struct FastSynthesisResult {
    symctrl::part2::ReachabilityController controller;
    symctrl::part2::StateSet winningInitialStates;
    std::uint64_t virtualEdges = 0;
    double abstractionSeconds = 0.0;
};

FastSynthesisResult solveUnicycleFast(
    const RobotConfig& config,
    const symctrl::IStateGrid& grid,
    const symctrl::IInputCatalog& inputs,
    const symctrl::IReachability& reachability,
    const symctrl::Box& disturbance,
    const symctrl::part2::Automaton& automaton,
    const symctrl::part2::StateLabels& labels) {
    using namespace symctrl;
    using namespace symctrl::part2;

    const auto absStart = std::chrono::steady_clock::now();

    const int nx1 = static_cast<int>(config.nx1);
    const int nx2 = static_cast<int>(config.nx2);
    const int nx3 = static_cast<int>(config.nx3);
    const double eta1 = (config.x1Max - config.x1Min) / static_cast<double>(nx1);
    const double eta2 = (config.x2Max - config.x2Min) / static_cast<double>(nx2);
    const double eta3 = (config.x3Max - config.x3Min) / static_cast<double>(nx3);

    const StateId S = grid.internalStateCount();
    const InputId A = inputs.inputCount();
    const std::size_t Q = automaton.transitions.size();
    const StateId total = S * Q;

    // 1. Precalcul des transitions relatives (invariance par translation en x1, x2)
    std::vector<RelativeTransition> table(static_cast<std::size_t>(nx3) * A);
    std::uint64_t virtualEdges = 0;

    for (int i3 = 0; i3 < nx3; ++i3) {
        // Cellule de reference (i1 = 0, i2 = 0, i3) en ordre dimension-0-fastest
        const StateId refState = 1ULL + static_cast<StateId>(i3) * nx1 * nx2;
        const Box refCell = grid.cellBounds(refState);

        for (InputId a = 0; a < A; ++a) {
            const Box enc = reachability.enclose(refCell, inputs.value(a), disturbance);
            RelativeTransition t;

            // Plage d'indices i1, i2 pour lesquels la boite reste strictement dans le domaine
            t.minI1 = 0;
            while (t.minI1 < nx1 && enc.lo[0] + t.minI1 * eta1 <= config.x1Min) ++t.minI1;
            t.maxI1 = nx1 - 1;
            while (t.maxI1 >= 0 && enc.hi[0] + t.maxI1 * eta1 >= config.x1Max) --t.maxI1;

            t.minI2 = 0;
            while (t.minI2 < nx2 && enc.lo[1] + t.minI2 * eta2 <= config.x2Min) ++t.minI2;
            t.maxI2 = nx2 - 1;
            while (t.maxI2 >= 0 && enc.hi[1] + t.maxI2 * eta2 >= config.x2Max) --t.maxI2;

            if (t.minI1 <= t.maxI1 && t.minI2 <= t.maxI2) {
                t.d1Lo = static_cast<int>(std::ceil((enc.lo[0] - config.x1Min) / eta1)) - 1;
                t.d1Hi = static_cast<int>(std::floor((enc.hi[0] - config.x1Min) / eta1));
                t.d2Lo = static_cast<int>(std::ceil((enc.lo[1] - config.x2Min) / eta2)) - 1;
                t.d2Hi = static_cast<int>(std::floor((enc.hi[1] - config.x2Min) / eta2));

                // Indices d'angle x3 (periodique)
                const long long f3 = static_cast<long long>(std::ceil((enc.lo[2] - config.x3Min) / eta3)) - 1LL;
                const long long l3 = static_cast<long long>(std::floor((enc.hi[2] - config.x3Min) / eta3));
                if (l3 - f3 + 1LL >= nx3) {
                    t.nextI3.resize(static_cast<std::size_t>(nx3));
                    for (int k = 0; k < nx3; ++k) t.nextI3[static_cast<std::size_t>(k)] = static_cast<std::uint32_t>(k);
                } else {
                    for (long long k = f3; k <= l3; ++k) {
                        long long w = k % nx3;
                        if (w < 0) w += nx3;
                        t.nextI3.push_back(static_cast<std::uint32_t>(w));
                    }
                    std::sort(t.nextI3.begin(), t.nextI3.end());
                    t.nextI3.erase(std::unique(t.nextI3.begin(), t.nextI3.end()), t.nextI3.end());
                }

                if (t.d1Lo <= t.d1Hi && t.d2Lo <= t.d2Hi && !t.nextI3.empty()) {
                    t.feasible = true;
                    const std::uint64_t validCells =
                        static_cast<std::uint64_t>(t.maxI1 - t.minI1 + 1)
                        * static_cast<std::uint64_t>(t.maxI2 - t.minI2 + 1);
                    const std::uint64_t deg =
                        static_cast<std::uint64_t>(t.d1Hi - t.d1Lo + 1)
                        * static_cast<std::uint64_t>(t.d2Hi - t.d2Lo + 1)
                        * static_cast<std::uint64_t>(t.nextI3.size());
                    virtualEdges += validCells * deg;
                }
            }
            table[static_cast<std::size_t>(i3) * A + a] = std::move(t);
        }
    }

    const double absSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - absStart).count();

    // 2. Table plate des labels et des rangs
    std::vector<LabelId> labelVec(static_cast<std::size_t>(S) + 1, 0);
    for (const auto& [s, l] : labels) {
        labelVec[static_cast<std::size_t>(s)] = l;
    }

    constexpr std::uint32_t kUnreached = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> ranks(static_cast<std::size_t>(total) + 1, kUnreached);
    std::vector<std::uint8_t> isFinal(Q, 0);

    for (AutomatonStateId q : automaton.finalStates) {
        isFinal[q] = 1;
        for (StateId s = 1; s <= S; ++s) {
            ranks[static_cast<std::size_t>(s + S * q)] = 0;
        }
    }

    const StateId strideI3 = static_cast<StateId>(nx1) * static_cast<StateId>(nx2);

    // Helper : verifie si tous les successeurs de (i1, i2, i3, q, a) ont un rang <= maxRank
    auto allSuccessorsWithin = [&](int i1, int i2, int i3,
                                   const std::vector<AutomatonStateId>& transRow,
                                   InputId a, std::uint32_t maxRank) -> bool {
        const auto& t = table[static_cast<std::size_t>(i3) * A + a];
        if (!t.feasible || i1 < t.minI1 || i1 > t.maxI1 || i2 < t.minI2 || i2 > t.maxI2) {
            return false;
        }
        for (std::uint32_t ni3 : t.nextI3) {
            const StateId base3 = 1ULL + static_cast<StateId>(ni3) * strideI3;
            for (int di2 = t.d2Lo; di2 <= t.d2Hi; ++di2) {
                const StateId base2 = base3 + static_cast<StateId>(i2 + di2) * nx1;
                for (int di1 = t.d1Lo; di1 <= t.d1Hi; ++di1) {
                    const StateId next = base2 + static_cast<StateId>(i1 + di1);
                    const auto nextQ = transRow[labelVec[static_cast<std::size_t>(next)]];
                    if (ranks[static_cast<std::size_t>(next + S * nextQ)] > maxRank) {
                        return false;
                    }
                }
            }
        }
        return true;
    };

    // 3. Point fixe d'atteignabilite
    std::uint32_t currentRank = 0;
    std::vector<StateId> newlyReached;
    while (true) {
        newlyReached.clear();
        for (std::size_t q = 0; q < Q; ++q) {
            if (isFinal[q]) continue;
            const auto& transRow = automaton.transitions[q];
            const StateId qOffset = S * q;

            for (int i3 = 0; i3 < nx3; ++i3) {
                const StateId base3 = 1ULL + static_cast<StateId>(i3) * strideI3;
                for (int i2 = 0; i2 < nx2; ++i2) {
                    const StateId base2 = base3 + static_cast<StateId>(i2) * nx1;
                    for (int i1 = 0; i1 < nx1; ++i1) {
                        const StateId id = qOffset + base2 + static_cast<StateId>(i1);
                        if (ranks[static_cast<std::size_t>(id)] != kUnreached) continue;

                        for (InputId a = 0; a < A; ++a) {
                            if (allSuccessorsWithin(i1, i2, i3, transRow, a, currentRank)) {
                                newlyReached.push_back(id);
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (newlyReached.empty()) break;
        ++currentRank;
        for (StateId id : newlyReached) {
            ranks[static_cast<std::size_t>(id)] = currentRank;
        }
    }

    // 4. Construction du controleur
    // 4. Construction du controleur (meilleure commande : rang moyen minimal + lissage)
    FastSynthesisResult res;
    res.virtualEdges = virtualEdges;
    res.abstractionSeconds = absSeconds;

    for (std::size_t q = 0; q < Q; ++q) {
        const auto& transRow = automaton.transitions[q];
        const StateId qOffset = S * q;

        for (int i3 = 0; i3 < nx3; ++i3) {
            const StateId base3 = 1ULL + static_cast<StateId>(i3) * strideI3;
            for (int i2 = 0; i2 < nx2; ++i2) {
                const StateId base2 = base3 + static_cast<StateId>(i2) * nx1;
                for (int i1 = 0; i1 < nx1; ++i1) {
                    const StateId id = qOffset + base2 + static_cast<StateId>(i1);
                    const std::uint32_t r = ranks[static_cast<std::size_t>(id)];
                    if (r == kUnreached) continue;
                    if (r == 0) {
                        res.controller[id] = {};
                        continue;
                    }

                    InputId bestAction = 0;
                    double bestScore = std::numeric_limits<double>::infinity();
                    bool found = false;

                    for (InputId a = 0; a < A; ++a) {
                        const auto& t = table[static_cast<std::size_t>(i3) * A + a];
                        if (!t.feasible || i1 < t.minI1 || i1 > t.maxI1 || i2 < t.minI2 || i2 > t.maxI2) {
                            continue;
                        }

                        bool valid = true;
                        std::uint64_t rankSum = 0;
                        std::uint64_t count = 0;

                        for (std::uint32_t ni3 : t.nextI3) {
                            const StateId nBase3 = 1ULL + static_cast<StateId>(ni3) * strideI3;
                            for (int di2 = t.d2Lo; di2 <= t.d2Hi; ++di2) {
                                const StateId nBase2 = nBase3 + static_cast<StateId>(i2 + di2) * nx1;
                                for (int di1 = t.d1Lo; di1 <= t.d1Hi; ++di1) {
                                    const StateId next = nBase2 + static_cast<StateId>(i1 + di1);
                                    const auto nextQ = transRow[labelVec[static_cast<std::size_t>(next)]];
                                    const std::uint32_t nr = ranks[static_cast<std::size_t>(next + S * nextQ)];
                                    if (nr >= r) {
                                        valid = false;
                                        break;
                                    }
                                    rankSum += nr;
                                    ++count;
                                }
                                if (!valid) break;
                            }
                            if (!valid) break;
                        }

                        if (!valid || count == 0) continue;

                        const auto& u = inputs.value(a);
                        const double avgRank = static_cast<double>(rankSum) / static_cast<double>(count);
                        // Critere : rang moyen minimal, puis rotation minimale |u2|, puis vitesse u1 maximale
                        const double score = avgRank + 0.05 * std::abs(u[1]) - 0.01 * u[0];

                        if (score < bestScore) {
                            bestScore = score;
                            bestAction = a;
                            found = true;
                        }
                    }

                    if (found) {
                        res.controller[id].push_back(bestAction);
                    }
                }
            }
        }
    }

    // 5. Etats initiaux gagnants Q0
    const auto& initRow = automaton.transitions[automaton.initialState];
    for (StateId s = 1; s <= S; ++s) {
        const auto q = initRow[labelVec[static_cast<std::size_t>(s)]];
        if (ranks[static_cast<std::size_t>(s + S * q)] != kUnreached) {
            res.winningInitialStates.insert(s);
        }
    }

    return res;
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
    gridSpec.periodic = {false, false, true}; // pour le robot unicycle, l'angle est périodique

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

    report(onProgress, 2, 5, "Compilation de l'automate et des labels...");
    Automaton automaton = compileMissionToAutomaton(config.regions, config.mission);
    StateLabels labels = buildRegionLabels(*grid, config.regions, config.mission);

    report(onProgress, 3, 5, "Abstraction rapide et synthese (produit implicite)...");
    auto solved = solveUnicycleFast(
        config, *grid, *inputs, *reachability, disturbance, automaton, labels);

    report(onProgress, 5, 5, "Assemblage de la politique symbolique...");
    StateSet winningInit = std::move(solved.winningInitialStates);

    auto policy = std::make_shared<AutomatonPolicy>(
        grid->internalStateCount(), inputs->inputCount(),
        automaton, labels, std::move(solved.controller));

    SynthesisOutput out;
    out.grid = std::move(grid);
    out.inputs = std::move(inputs);
    out.dynamics = std::move(dynamics);
    out.automaton = std::move(automaton);
    out.labels = std::move(labels);
    out.winningInitialStates = std::move(winningInit);
    out.policy = std::move(policy);
    out.storedEdges = solved.virtualEdges;
    out.abstractionSeconds = solved.abstractionSeconds;
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