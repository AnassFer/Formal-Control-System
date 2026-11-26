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

enum class SolverMode {
    BitsetWitness,   // Monolithic 1-bit/state dense bitset + exact-integer witness verification (no graph)
    CompactWitness,  // Assume-guarantee local witness / constant temoin certificate (O(1) memory, any N >= 3)
    ExplicitGraph    // Full explicit ExactRingBuildingTransitionSystem + symctrl::part2 safety synthesis
};

const char* toString(SolverMode mode) noexcept;

struct ChallengeConfig {
    std::size_t rooms = 3;
    symctrl::ReachabilityMethod reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
    SolverMode solverMode = SolverMode::BitsetWitness;
    std::uint64_t memoryBudgetMiB = 16384; // 16 GiB safeguard
};

struct ScaleEstimate {
    std::size_t rooms = 0;
    SolverMode solverMode = SolverMode::BitsetWitness;
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
    Witness,     // Certified witness action (constant 0.3 if globally valid, else max-margin local witness)
    MinEnergy,   // Lowest-energy certified safe input
    FirstSafe,   // First certified safe input
    MaxHeating   // Highest-heating certified safe input
};

struct LocalWitnessCertificate {
    // Indexed by (cPrev * 10 + cCur) * 10 + cNext in [0, 999].
    std::array<std::uint8_t, 1000> localSafeMask{};     // 3-bit mask of non-OUT local inputs q in {0,1,2}
    std::array<std::uint8_t, 1000> bestLocalWitness{};  // Preferred local input q in {0,1,2} (0xFF if none)
    int uniformWitnessQ = -1;                           // >= 0 if constant u_i = kHeaterInputs[q] certifies all triplets
    bool allTripletsHaveSafeAction = false;
    bool allStatesSafe = false;                         // True iff R* = {1, ..., 10^N} (100% controllable)
    symctrl::ReachabilityMethod reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
};

struct ChallengeMetrics {
    std::size_t rooms = 0;
    SolverMode solverMode = SolverMode::BitsetWitness;
    bool statesFitUint64 = true;
    std::uint64_t internalStates = 0;
    double internalStatesApprox = 0.0;
    bool inputsFitUint64 = true;
    std::uint64_t inputCount = 0;
    double inputCountApprox = 0.0;
    std::uint64_t processedPairs = 0;
    std::uint64_t storedEdges = 0;
    std::uint64_t controllableStates = 0;
    double controllablePercent = 0.0;
    std::uint32_t fixedPointIterations = 0;
    int uniformWitnessQ = -1;
    double abstractionSeconds = 0.0;
    double synthesisSeconds = 0.0;
    double totalSeconds = 0.0;
    double peakMemoryMiB = 0.0;
    double currentMemoryMiB = 0.0;
    bool memoryMeasured = false;
    bool isolatedProcess = false;
    symctrl::NumericGuarantee numericGuarantee = symctrl::NumericGuarantee::ValidatedEnclosure;
    symctrl::ReachabilityMethod reachabilityMethod = symctrl::ReachabilityMethod::CooperativeMonotone;
};

struct ChallengeSynthesisOutput {
    std::shared_ptr<const symctrl::IStateGrid> grid;
    std::shared_ptr<const symctrl::IInputCatalog> inputs;
    std::shared_ptr<const symctrl::IDynamics> dynamics;
    std::shared_ptr<const symctrl::ITransitionSystem> model; // Populated in ExplicitGraph mode
    symctrl::part2::StateSet safetyDomain;                   // Populated in ExplicitGraph mode
    symctrl::part2::SafetyController controller;             // Populated in ExplicitGraph mode
    std::vector<std::uint64_t> safeBitset;                   // Populated in BitsetWitness mode when !allStatesSafe
    LocalWitnessCertificate witnessCert;                     // Populated in BitsetWitness & CompactWitness modes
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

// Adapts ExplicitGraph SafetyController to symctrl::ISymbolicPolicy.
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

// Adapts BitsetWitness / CompactWitness certificate to symctrl::ISymbolicPolicy
// without storing a 10^N controller table or 3^N input vectors.
class BitsetWitnessPolicy final : public symctrl::ISymbolicPolicy {
public:
    BitsetWitnessPolicy(std::size_t rooms,
                        const LocalWitnessCertificate& cert,
                        const std::vector<std::uint64_t>& safeBitset,
                        ActionStrategy strategy = ActionStrategy::Witness);

    symctrl::PolicyDecision decide(symctrl::PolicyMemory previous_memory,
                                   symctrl::StateId current_state) const override;

private:
    std::size_t rooms_;
    LocalWitnessCertificate cert_;
    const std::vector<std::uint64_t>& safeBitset_;
    ActionStrategy strategy_;
    std::vector<symctrl::InputId> inputStrides_; // 3^d
};

ScaleEstimate estimateChallengeScale(std::size_t rooms,
                                     SolverMode mode = SolverMode::BitsetWitness);

MemorySnapshot queryProcessMemory();

// Runs the certified circular-building abstraction and safety synthesis in-process.
ChallengeSynthesisOutput runBuildingChallenge(const ChallengeConfig& config,
                                              const ProgressCallback& onProgress = {});

// Runs synthesis in a fresh isolated `challenge_cli` subprocess so Peak RSS is measured
// independently per run without GUI overhead or high-water carry-over from prior runs.
ChallengeSynthesisOutput runBuildingChallengeIsolated(const ChallengeConfig& config,
                                                      const ProgressCallback& onProgress = {});

void saveSafetyControllerBinary(const ChallengeSynthesisOutput& synth,
                                const std::filesystem::path& filePath);

void loadSafetyControllerBinary(const std::filesystem::path& filePath,
                                std::size_t expectedRooms,
                                ChallengeSynthesisOutput& outSynth);

BuildingTrajectory simulateBuildingClosedLoop(const ChallengeSynthesisOutput& synth,
                                              const symctrl::Vector& initialTemperatures,
                                              std::size_t horizon = 40,
                                              ActionStrategy strategy = ActionStrategy::Witness);

void exportBuildingTrajectoryCsv(const BuildingTrajectory& trajectory,
                                 const std::filesystem::path& filePath);

void exportChallengeModelPackage(const ChallengeSynthesisOutput& synth,
                                 const std::filesystem::path& directory);

} // namespace challenge_app
