#pragma once

#include "mission_compiler.hpp"
#include "unicycle_dynamics.hpp"

#include "symctrl/input_catalog.hpp"
#include "symctrl/state_grid.hpp"
#include "symctrl/synthesis/automaton_policy.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace robot_app {

struct RobotConfig {
    double x1Min = 0.0;
    double x1Max = 10.0;
    double x2Min = 0.0;
    double x2Max = 10.0;
    double x3Min = -kPi;
    double x3Max = kPi;

    std::uint32_t nx1 = 20;
    std::uint32_t nx2 = 20;
    std::uint32_t nx3 = 12;

    double u1Min = 0.25;
    double u1Max = 1.0;
    double u2Min = -1.0;
    double u2Max = 1.0;

    std::size_t nu1 = 3;
    std::size_t nu2 = 5;

    double w1Max = 0.05;
    double w2Max = 0.05;
    double w3Max = 0.05;
    double tau = 1.0;

    std::vector<RegionSpec> regions;
    MissionSpec mission;
};

struct SynthesisOutput {
    std::shared_ptr<const symctrl::IStateGrid> grid;
    std::shared_ptr<const symctrl::IInputCatalog> inputs;
    std::shared_ptr<const symctrl::IDynamics> dynamics;
    symctrl::part2::Automaton automaton;
    symctrl::part2::StateLabels labels;
    symctrl::part2::StateSet winningInitialStates;
    std::shared_ptr<symctrl::part2::AutomatonPolicy> policy;
    std::uint64_t storedEdges = 0;
    double abstractionSeconds = 0.0;
    double totalSeconds = 0.0;
};

struct TrajectoryPoint {
    std::size_t step = 0;
    double x1 = 0.0;
    double x2 = 0.0;
    double x3 = 0.0;
    double u1 = 0.0;
    double u2 = 0.0;
    std::uint64_t memory = 0;
};

struct Trajectory {
    bool success = false;
    std::string statusMessage;
    std::vector<TrajectoryPoint> points;
};

using ProgressCallback = std::function<void(int step, int totalSteps, const std::string& message)>;

RobotConfig makePaperDefaultConfig(bool fullPaperGrid = false);

SynthesisOutput runRobotSynthesis(const RobotConfig& config,
                                  const ProgressCallback& onProgress = {});

Trajectory simulateClosedLoop(const SynthesisOutput& synth,
                              const RobotConfig& config,
                              const symctrl::Vector& x0,
                              std::uint32_t seed,
                              std::size_t maxSteps = 200);

void exportTrajectoryCsv(const Trajectory& trajectory, const std::string& filePath);

} // namespace robot_app