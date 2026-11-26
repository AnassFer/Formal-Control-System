#pragma once

#include "building_challenge.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace challenge_app::ui {

struct PlottedBuildingRun {
    BuildingTrajectory raw;
    std::vector<double> timeSteps;
    std::vector<double> inputTimeSteps;
    std::vector<std::vector<double>> roomTemperatures; // [room][step]
    std::vector<std::vector<double>> roomInputs;       // [room][step]
};

class ChallengeUiState {
public:
    ChallengeUiState();
    ~ChallengeUiState();

    ChallengeUiState(const ChallengeUiState&) = delete;
    ChallengeUiState& operator=(const ChallengeUiState&) = delete;

    // User-controlled inputs (unknown N >= 3)
    int roomsInput = 3;
    int solverModeIndex = 0;   // 0 = BitsetWitness, 1 = CompactWitness (Temoin), 2 = ExplicitGraph
    int reachabilityIndex = 0; // 0 = CooperativeMonotone, 1 = BoundedDerivative
    int memoryBudgetMiB = 16384;

    // Simulation controls
    std::vector<double> initialTemperatures;
    int horizon = 40;
    int strategyIndex = 0; // 0 = Witness, 1 = MinEnergy, 2 = FirstSafe, 3 = MaxHeating

    // Results and benchmark log
    std::optional<ChallengeSynthesisOutput> synthesis;
    std::optional<PlottedBuildingRun> plottedRun;
    std::vector<ChallengeMetrics> history;
    std::string lastError;
    std::string lastStatus;
    bool configDirty = true;

    SolverMode selectedSolverMode() const noexcept;

    void syncInitialTemperaturesToN();
    void applyUniformInitial(double temp);
    void applyAlternatingInitial(double lowTemp, double highTemp);

    void startSynthesis();
    void pollSynthesis();
    bool isSynthesisRunning() const noexcept;
    int progressStep() const noexcept;
    int progressTotal() const noexcept;
    std::string progressMessage() const;

    void runSimulation();
    void exportCsv(const std::string& path);

private:
    void joinWorkerIfFinished();

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int> progressStep_{0};
    std::atomic<int> progressTotal_{5};

    mutable std::mutex mutex_;
    std::string progressMessage_ = "Ready.";
    bool hasPendingResult_ = false;
    std::optional<ChallengeSynthesisOutput> pendingSynthesis_;
    std::string pendingError_;
};

} // namespace challenge_app::ui
