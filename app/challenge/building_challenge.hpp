#pragma once

#include "symctrl/dynamics.hpp"
#include "symctrl/input_catalog.hpp"
#include "symctrl/policy.hpp"
#include "symctrl/reachable_enclosure.hpp"
#include "symctrl/state_grid.hpp"
#include "symctrl/synthesis/safety_controller.hpp"
#include "symctrl/transition_system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace challenge_app {

// Fixed competition parameters from Saoud, "Project - Symbolic Control" (Slides 14-16)
inline constexpr double kTempMin = 17.0;
inline constexpr double kTempMax = 22.0;
inline constexpr std::uint32_t kCellsPerDimension = 10;
inline constexpr std::array<double, 3> kHeaterInputs = {0.0, 0.3, 0.6};
inline constexpr double kAlpha = 0.45;
inline constexpr double kBeta = 0.045;
inline constexpr double kGamma = 0.09;
inline constexpr double kOutsideTemp = -1.0;
inline constexpr double kHeaterTemp = 50.0;

struct ChallengeConfig {
    std::size_t rooms = 3;
    symctrl::ReachabilityMethod reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
    std::uint64_t memoryBudgetMiB = 16384; // 16 GiB safeguard for explicit graph construction
};

struct ScaleEstimate {
    std::size_t rooms = 0;
    bool statesFitUint64 = false;
    bool inputsFitUint64 = false;
    bool pairsFitUint64 = false;
    std::uint64_t symbolicStates = 0;
    std::uint64_t symbolicInputs = 0;
    std::uint64_t stateInputPairs = 0;
    double statesApprox = 0.0;
    double inputsApprox = 0.0;
    double pairsApprox = 0.0;
    double estimatedEdgesApprox = 0.0;
    double estimatedRamMiB = 0.0;
};

struct MemorySnapshot {
    double currentRssMiB = 0.0;
    double peakRssMiB = 0.0;
    bool available = false;
};

enum class ActionStrategy {
    MinEnergy,
    FirstSafe,
    MaxHeating
};

struct ChallengeMetrics {
    std::size_t rooms = 0;
    std::uint64_t internalStates = 0;
    std::uint64_t inputCount = 0;
    std::uint64_t processedPairs = 0;
    std::uint64_t storedEdges = 0;
    std::uint64_t controllableStates = 0;
    double controllablePercent = 0.0;
    double abstractionSeconds = 0.0;
    double synthesisSeconds = 0.0;
    double totalSeconds = 0.0;
    double peakMemoryMiB = 0.0;
    double currentMemoryMiB = 0.0;
    bool memoryMeasured = false;
    symctrl::NumericGuarantee numericGuarantee = symctrl::NumericGuarantee::UncertifiedFloat;
    symctrl::ReachabilityMethod reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
};

struct ChallengeSynthesisOutput {
    std::shared_ptr<const symctrl::IStateGrid> grid;
    std::shared_ptr<const symctrl::IInputCatalog> inputs;
    std::shared_ptr<const symctrl::IDynamics> dynamics;
    std::shared_ptr<const symctrl::ITransitionSystem> model;
    symctrl::part2::StateSet safetyDomain;
    symctrl::part2::SafetyController controller;
    ChallengeMetrics metrics;
};

struct BuildingTrajectoryStep {
    std::size_t step = 0;
    symctrl::StateId stateId = symctrl::kOutside;
    symctrl::InputId inputId = 0;
    bool hasInput = false;
    bool safe = false;
    symctrl::Vector temperatures;
    symctrl::Vector heaterInputs;
};

struct BuildingTrajectory {
    bool allStepsSafe = false;
    std::string statusMessage;
    std::vector<BuildingTrajectoryStep> steps;
};

using ProgressCallback = std::function<void(int step, int totalSteps, const std::string& message)>;

// Adapts the synthesized multivalued safety controller to symctrl::ISymbolicPolicy
// so closed-loop control uses symctrl::refineDecision directly.
class SafetyTablePolicy final : public symctrl::ISymbolicPolicy {
public:
    SafetyTablePolicy(const symctrl::part2::SafetyController& controller,
                      const symctrl::IInputCatalog& inputs,
                      ActionStrategy strategy = ActionStrategy::MinEnergy);

    symctrl::PolicyDecision decide(symctrl::PolicyMemory previous_memory,
                                   symctrl::StateId current_state) const override;

private:
    const symctrl::part2::SafetyController& controller_;
    const symctrl::IInputCatalog& inputs_;
    ActionStrategy strategy_;
};

ScaleEstimate estimateChallengeScale(std::size_t rooms);

MemorySnapshot queryProcessMemory();

ChallengeSynthesisOutput runBuildingChallenge(const ChallengeConfig& config,
                                              const ProgressCallback& onProgress = {});

BuildingTrajectory simulateBuildingClosedLoop(const ChallengeSynthesisOutput& synth,
                                              const symctrl::Vector& initialTemperatures,
                                              std::size_t horizon = 40,
                                              ActionStrategy strategy = ActionStrategy::MinEnergy);

void exportBuildingTrajectoryCsv(const BuildingTrajectory& trajectory,
                                 const std::filesystem::path& filePath);

void exportChallengeModelPackage(const ChallengeSynthesisOutput& synth,
                                 const std::filesystem::path& directory);

} // namespace challenge_app
