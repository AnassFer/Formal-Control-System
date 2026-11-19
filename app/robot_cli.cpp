#include "robot_simulation.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main() {
    try {
        const robot_app::RobotConfig config = robot_app::makePaperDefaultConfig(false);

        const auto synth = robot_app::runRobotSynthesis(
            config, [](int step, int total, const std::string& msg) {
                std::cout << '[' << step << '/' << total << "] " << msg << '\n';
            });

        std::cout << "Synthese terminee en " << synth.totalSeconds << " s "
                  << "(abstraction: " << synth.abstractionSeconds << " s, "
                  << "aretes: " << synth.storedEdges << ")\n"
                  << "Etats initiaux gagnants |Q0| = " << synth.winningInitialStates.size()
                  << " / " << synth.grid->internalStateCount() << '\n';

        const std::vector<symctrl::Vector> fig6Inits = {
            {0.5, 4.5, 0.0},
            {0.5, 4.0, 0.0}
        };
        for (std::size_t i = 0; i < fig6Inits.size(); ++i) {
            const auto traj = robot_app::simulateClosedLoop(
                synth, config, fig6Inits[i], static_cast<std::uint32_t>(42 + i));
            std::cout << "Fig 6 traj " << i << " : "
                      << (traj.success ? "SUCCES" : "ECHEC")
                      << " (" << traj.statusMessage << ")\n";
            if (!traj.points.empty()) {
                robot_app::exportTrajectoryCsv(traj, "fig6_traj_" + std::to_string(i) + ".csv");
            }
        }

        std::size_t okCount = 0;
        const symctrl::Vector fig7Init{6.8, 9.0, 0.0};
        for (std::uint32_t k = 0; k < 50; ++k) {
            const auto traj = robot_app::simulateClosedLoop(synth, config, fig7Init, 100 + k);
            if (traj.success) {
                ++okCount;
            }
        }
        std::cout << "Fig 7 (6.8, 9.0, 0.0) : " << okCount << " / 50 reussites\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Erreur : " << e.what() << '\n';
        return 1;
    }
}