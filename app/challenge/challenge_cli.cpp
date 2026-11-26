#include "building_challenge.hpp"

#include <cctype>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage: challenge_cli [N] [monotone|derivative] [ram_budget_mib] [bitset|temoin|explicit]\n"
              << "  N              Number of circular-building rooms (exact integer >= 3, default: 3)\n"
              << "  method         Reachability method: monotone (default) or derivative\n"
              << "  ram_budget_mib Memory budget in MiB (exact integer >= 1, default: 16384)\n"
              << "  solver         Solver mode: bitset (default), temoin (O(1) witness), or explicit\n";
}

std::uint64_t parseStrictUint64(const std::string& raw, const char* paramName) {
    if (raw.empty()) {
        throw std::invalid_argument(std::string(paramName) + " must be a positive integer.");
    }
    for (char ch : raw) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            throw std::invalid_argument(
                std::string("Invalid integer for ") + paramName + ": '" + raw + "'");
        }
    }
    std::size_t pos = 0;
    std::uint64_t value = 0;
    try {
        value = std::stoull(raw, &pos, 10);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            std::string("Out-of-range integer for ") + paramName + ": '" + raw + "'");
    }
    if (pos != raw.size()) {
        throw std::invalid_argument(
            std::string("Invalid trailing characters for ") + paramName + ": '" + raw + "'");
    }
    return value;
}

symctrl::ReachabilityMethod parseMethod(const std::string& raw) {
    if (raw == "monotone") {
        return symctrl::ReachabilityMethod::CooperativeMonotone;
    }
    if (raw == "derivative") {
        return symctrl::ReachabilityMethod::BoundedDerivative;
    }
    throw std::invalid_argument("Unknown reachability method: '" + raw
                                + "' (expected 'monotone' or 'derivative').");
}

challenge_app::SolverMode parseSolverMode(const std::string& raw) {
    if (raw == "bitset") {
        return challenge_app::SolverMode::BitsetWitness;
    }
    if (raw == "temoin" || raw == "witness") {
        return challenge_app::SolverMode::CompactWitness;
    }
    if (raw == "explicit") {
        return challenge_app::SolverMode::ExplicitGraph;
    }
    throw std::invalid_argument("Unknown solver mode: '" + raw
                                + "' (expected 'bitset', 'temoin', or 'explicit').");
}

std::size_t parseRooms(const std::string& raw) {
    const std::uint64_t val = parseStrictUint64(raw, "N");
    if (val < 3 || val > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::invalid_argument("N must be an integer >= 3 (got '" + raw + "').");
    }
    return static_cast<std::size_t>(val);
}

std::uint64_t parseBudgetMiB(const std::string& raw) {
    const std::uint64_t val = parseStrictUint64(raw, "ram_budget_mib");
    if (val == 0) {
        throw std::invalid_argument("ram_budget_mib must be >= 1.");
    }
    return val;
}

int runIsolatedWorker(int argc, char** argv) {
    // Internal protocol:
    //   challenge_cli --isolated-worker <N> <method> <budget_mib> <solver> <out_bin_path>
    try {
        if (argc != 7) {
            throw std::invalid_argument("Invalid arguments for --isolated-worker.");
        }
        challenge_app::ChallengeConfig config;
        config.rooms = parseRooms(argv[2]);
        config.reachabilityMethod = parseMethod(argv[3]);
        config.memoryBudgetMiB = parseBudgetMiB(argv[4]);
        config.solverMode = parseSolverMode(argv[5]);
        const std::filesystem::path outPath(argv[6]);

        const auto output = challenge_app::runBuildingChallenge(
            config, [](int step, int total, const std::string& msg) {
                std::cout << "PROGRESS:" << step << ':' << total << ':' << msg << '\n' << std::flush;
            });

        challenge_app::saveSafetyControllerBinary(output, outPath);

        const auto& m = output.metrics;
        std::cout.imbue(std::locale::classic());
        std::cout << std::setprecision(12)
                  << "METRIC:states_fit_u64=" << (m.statesFitUint64 ? 1 : 0) << '\n'
                  << "METRIC:internal_states=" << m.internalStates << '\n'
                  << "METRIC:internal_states_approx=" << m.internalStatesApprox << '\n'
                  << "METRIC:inputs_fit_u64=" << (m.inputsFitUint64 ? 1 : 0) << '\n'
                  << "METRIC:input_count=" << m.inputCount << '\n'
                  << "METRIC:input_count_approx=" << m.inputCountApprox << '\n'
                  << "METRIC:processed_pairs=" << m.processedPairs << '\n'
                  << "METRIC:stored_edges=" << m.storedEdges << '\n'
                  << "METRIC:controllable_states=" << m.controllableStates << '\n'
                  << "METRIC:controllable_percent=" << m.controllablePercent << '\n'
                  << "METRIC:fixed_point_iterations=" << m.fixedPointIterations << '\n'
                  << "METRIC:uniform_witness_q=" << m.uniformWitnessQ << '\n'
                  << "METRIC:abstraction_seconds=" << m.abstractionSeconds << '\n'
                  << "METRIC:synthesis_seconds=" << m.synthesisSeconds << '\n'
                  << "METRIC:total_seconds=" << m.totalSeconds << '\n'
                  << "METRIC:peak_memory_mib=" << m.peakMemoryMiB << '\n'
                  << "METRIC:current_memory_mib=" << m.currentMemoryMiB << '\n'
                  << "METRIC:memory_measured=" << (m.memoryMeasured ? 1 : 0) << '\n'
                  << "METRIC:numeric_guarantee=" << symctrl::toString(m.numericGuarantee) << '\n'
                  << std::flush;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "ERROR:" << e.what() << '\n' << std::flush;
        return 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--isolated-worker") {
        return runIsolatedWorker(argc, argv);
    }

    try {
        challenge_app::ChallengeConfig config;

        if (argc >= 2) {
            const std::string arg1 = argv[1];
            if (arg1 == "--help" || arg1 == "-h") {
                if (argc != 2) {
                    throw std::invalid_argument("Unexpected extra arguments after --help.");
                }
                printUsage();
                return 0;
            }
            if (argc > 5) {
                throw std::invalid_argument(
                    "Too many arguments (expected at most 4: [N] [monotone|derivative] [ram_budget_mib] [bitset|temoin|explicit]).");
            }
            config.rooms = parseRooms(arg1);
        }

        if (argc >= 3) {
            config.reachabilityMethod = parseMethod(argv[2]);
        }

        if (argc >= 4) {
            config.memoryBudgetMiB = parseBudgetMiB(argv[3]);
        }

        if (argc >= 5) {
            config.solverMode = parseSolverMode(argv[4]);
        }

        const auto output = challenge_app::runBuildingChallenge(
            config, [](int step, int total, const std::string& msg) {
                std::cout << '[' << step << '/' << total << "] " << msg << '\n';
            });

        const auto& m = output.metrics;
        std::cout << std::fixed << std::setprecision(4)
                  << "\n=== Circular Building Safety Challenge Report ===\n"
                  << "N (rooms)            : " << m.rooms << '\n'
                  << "Solver mode          : " << challenge_app::toString(m.solverMode) << '\n'
                  << "Symbolic states 10^N : ";
        if (m.statesFitUint64) {
            std::cout << m.internalStates << '\n';
        } else {
            std::cout << std::scientific << std::setprecision(3) << m.internalStatesApprox << std::fixed << std::setprecision(4) << '\n';
        }
        std::cout << "Symbolic inputs 3^N  : ";
        if (m.inputsFitUint64) {
            std::cout << m.inputCount << '\n';
        } else {
            std::cout << std::scientific << std::setprecision(3) << m.inputCountApprox << std::fixed << std::setprecision(4) << '\n';
        }
        std::cout << "Stored edges         : " << m.storedEdges
                  << (m.solverMode == challenge_app::SolverMode::ExplicitGraph ? "" : " (matrix-free)") << '\n'
                  << "Fixed-point iters    : " << m.fixedPointIterations << '\n'
                  << "Uniform witness u_i  : "
                  << (m.uniformWitnessQ >= 0
                          ? std::to_string(challenge_app::kHeaterInputs[static_cast<std::size_t>(m.uniformWitnessQ)])
                          : "none")
                  << '\n'
                  << "T_abs (s)            : " << m.abstractionSeconds << '\n'
                  << "T_ctrl (s)           : " << m.synthesisSeconds << '\n'
                  << "Total T_abs+T_ctrl   : " << m.totalSeconds << '\n'
                  << "Controllable states  : ";
        if (m.statesFitUint64) {
            std::cout << m.controllableStates << " / " << m.internalStates << " ";
        }
        std::cout << "(" << std::setprecision(2) << m.controllablePercent << "%)\n"
                  << "Peak memory RSS      : "
                  << (m.memoryMeasured ? (std::to_string(m.peakMemoryMiB) + " MiB") : "unavailable")
                  << '\n'
                  << "Numeric guarantee    : " << symctrl::toString(m.numericGuarantee) << '\n';

        // Quick closed-loop verification starting near the cold boundary (17.2 C)
        symctrl::Vector x0(config.rooms, 17.2);
        const auto sim = challenge_app::simulateBuildingClosedLoop(
            output, x0, 30, challenge_app::ActionStrategy::Witness);
        std::cout << "Closed-loop test (30 steps from 17.2 C): " << sim.statusMessage << '\n';
        return sim.allStepsSafe ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
