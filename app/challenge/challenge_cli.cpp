#include "building_challenge.hpp"

#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage: challenge_cli [N] [monotone|derivative] [ram_budget_mib]\n"
              << "  N              Number of circular-building rooms (integer >= 3, default: 3)\n"
              << "  method         Reachability backend: monotone (default) or derivative\n"
              << "  ram_budget_mib Explicit graph memory budget in MiB (default: 16384)\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        challenge_app::ChallengeConfig config;

        if (argc >= 2) {
            const std::string arg1 = argv[1];
            if (arg1 == "--help" || arg1 == "-h") {
                printUsage();
                return 0;
            }
            const long long parsedN = std::stoll(arg1);
            if (parsedN < 3) {
                throw std::invalid_argument("N must be an integer >= 3.");
            }
            config.rooms = static_cast<std::size_t>(parsedN);
        }

        if (argc >= 3) {
            const std::string method = argv[2];
            if (method == "monotone") {
                config.reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
            } else if (method == "derivative") {
                config.reachabilityMethod = symctrl::ReachabilityMethod::BoundedDerivative;
            } else {
                throw std::invalid_argument("Unknown reachability method: " + method);
            }
        }

        if (argc >= 4) {
            const long long budget = std::stoll(argv[3]);
            if (budget <= 0) {
                throw std::invalid_argument("ram_budget_mib must be positive.");
            }
            config.memoryBudgetMiB = static_cast<std::uint64_t>(budget);
        }

        const auto output = challenge_app::runBuildingChallenge(
            config, [](int step, int total, const std::string& msg) {
                std::cout << '[' << step << '/' << total << "] " << msg << '\n';
            });

        const auto& m = output.metrics;
        std::cout << std::fixed << std::setprecision(4)
                  << "\n=== Circular Building Safety Challenge Report ===\n"
                  << "N (rooms)            : " << m.rooms << '\n'
                  << "Symbolic states 10^N : " << m.internalStates << '\n'
                  << "Symbolic inputs 3^N  : " << m.inputCount << '\n'
                  << "Stored edges         : " << m.storedEdges << '\n'
                  << "T_abs (s)            : " << m.abstractionSeconds << '\n'
                  << "T_ctrl (s)           : " << m.synthesisSeconds << '\n'
                  << "Total T_abs+T_ctrl   : " << m.totalSeconds << '\n'
                  << "Controllable states  : " << m.controllableStates << " / " << m.internalStates
                  << " (" << std::setprecision(2) << m.controllablePercent << "%)\n"
                  << "Peak memory RSS      : "
                  << (m.memoryMeasured ? (std::to_string(m.peakMemoryMiB) + " MiB") : "unavailable")
                  << '\n'
                  << "Numeric guarantee    : " << symctrl::toString(m.numericGuarantee) << '\n';

        // Quick closed-loop verification starting near the cold boundary (17.2 C)
        symctrl::Vector x0(config.rooms, 17.2);
        const auto sim = challenge_app::simulateBuildingClosedLoop(
            output, x0, 30, challenge_app::ActionStrategy::MinEnergy);
        std::cout << "Closed-loop test (30 steps from 17.2 C): " << sim.statusMessage << '\n';
        return sim.allStepsSafe ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
