#include "challenge_state.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace challenge_app::ui {

ChallengeUiState::ChallengeUiState() {
    syncInitialTemperaturesToN();
}

ChallengeUiState::~ChallengeUiState() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ChallengeUiState::syncInitialTemperaturesToN() {
    if (roomsInput < 3) {
        roomsInput = 3;
    }
    const auto targetSize = static_cast<std::size_t>(roomsInput);
    if (initialTemperatures.size() != targetSize) {
        const std::size_t oldSize = initialTemperatures.size();
        initialTemperatures.resize(targetSize, 19.5);
        for (std::size_t i = oldSize; i < targetSize; ++i) {
            // Spread defaults mildly across [17.5, 21.5] so room curves are distinguishable.
            const double phase = static_cast<double>(i % 5) / 4.0;
            initialTemperatures[i] = 17.5 + 4.0 * phase;
        }
    }
}

void ChallengeUiState::applyUniformInitial(double temp) {
    syncInitialTemperaturesToN();
    std::fill(initialTemperatures.begin(), initialTemperatures.end(), temp);
}

void ChallengeUiState::applyAlternatingInitial(double lowTemp, double highTemp) {
    syncInitialTemperaturesToN();
    for (std::size_t i = 0; i < initialTemperatures.size(); ++i) {
        initialTemperatures[i] = (i % 2 == 0) ? lowTemp : highTemp;
    }
}

void ChallengeUiState::joinWorkerIfFinished() {
    if (!running_.load() && worker_.joinable()) {
        worker_.join();
    }
}

void ChallengeUiState::startSynthesis() {
    if (running_.load()) {
        return;
    }
    joinWorkerIfFinished();
    syncInitialTemperaturesToN();

    lastError.clear();
    lastStatus.clear();
    plottedRun.reset();

    ChallengeConfig cfg;
    cfg.rooms = static_cast<std::size_t>(roomsInput);
    cfg.reachabilityMethod = (reachabilityIndex == 0)
        ? symctrl::ReachabilityMethod::CooperativeMonotone
        : symctrl::ReachabilityMethod::BoundedDerivative;
    cfg.memoryBudgetMiB = static_cast<std::uint64_t>(std::max(128, memoryBudgetMiB));

    running_.store(true);
    progressStep_.store(0);
    progressTotal_.store(5);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progressMessage_ = "Starting synthesis for N=" + std::to_string(cfg.rooms) + "...";
        hasPendingResult_ = false;
        pendingSynthesis_.reset();
        pendingError_.clear();
    }

    worker_ = std::thread([this, cfg]() {
        try {
            ChallengeSynthesisOutput output = runBuildingChallengeIsolated(
                cfg, [this](int step, int total, const std::string& msg) {
                    progressStep_.store(step);
                    progressTotal_.store(total);
                    std::lock_guard<std::mutex> lock(mutex_);
                    progressMessage_ = msg;
                });

            std::lock_guard<std::mutex> lock(mutex_);
            pendingSynthesis_ = std::move(output);
            progressMessage_ = "Synthesis complete.";
            hasPendingResult_ = true;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingError_ = e.what();
            progressMessage_ = "Synthesis failed.";
            hasPendingResult_ = true;
        }
        running_.store(false);
    });
}

void ChallengeUiState::pollSynthesis() {
    joinWorkerIfFinished();

    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasPendingResult_) {
        return;
    }
    if (!pendingError_.empty()) {
        lastError = pendingError_;
        pendingError_.clear();
    } else if (pendingSynthesis_.has_value()) {
        synthesis = std::move(pendingSynthesis_);
        history.push_back(synthesis->metrics);
        configDirty = false;
        lastError.clear();
        lastStatus = "Controller ready for N=" + std::to_string(synthesis->metrics.rooms) + ".";
    }
    pendingSynthesis_.reset();
    hasPendingResult_ = false;
}

bool ChallengeUiState::isSynthesisRunning() const noexcept {
    return running_.load();
}

int ChallengeUiState::progressStep() const noexcept {
    return progressStep_.load();
}

int ChallengeUiState::progressTotal() const noexcept {
    return progressTotal_.load();
}

std::string ChallengeUiState::progressMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progressMessage_;
}

void ChallengeUiState::runSimulation() {
    lastError.clear();
    plottedRun.reset();

    if (!synthesis.has_value()) {
        lastError = "Synthesize a safety controller first.";
        return;
    }

    const std::size_t n = synthesis->metrics.rooms;
    if (initialTemperatures.size() != n) {
        roomsInput = static_cast<int>(n);
        syncInitialTemperaturesToN();
    }

    const auto steps = static_cast<std::size_t>(std::max(1, horizon));
    ActionStrategy strategy = ActionStrategy::MinEnergy;
    if (strategyIndex == 1) {
        strategy = ActionStrategy::FirstSafe;
    } else if (strategyIndex == 2) {
        strategy = ActionStrategy::MaxHeating;
    }

    try {
        BuildingTrajectory traj = simulateBuildingClosedLoop(
            *synthesis, initialTemperatures, steps, strategy);

        PlottedBuildingRun plotted;
        plotted.roomTemperatures.assign(n, {});
        plotted.roomInputs.assign(n, {});

        for (const auto& pt : traj.steps) {
            plotted.timeSteps.push_back(static_cast<double>(pt.step));
            for (std::size_t i = 0; i < n; ++i) {
                plotted.roomTemperatures[i].push_back(pt.temperatures[i]);
            }
            if (pt.hasInput) {
                plotted.inputTimeSteps.push_back(static_cast<double>(pt.step));
                for (std::size_t i = 0; i < n; ++i) {
                    plotted.roomInputs[i].push_back(pt.heaterInputs[i]);
                }
            }
        }

        lastStatus = traj.statusMessage;
        plotted.raw = std::move(traj);
        plottedRun = std::move(plotted);
    } catch (const std::exception& e) {
        lastError = e.what();
    }
}

void ChallengeUiState::exportCsv(const std::string& path) {
    if (!plottedRun.has_value()) {
        lastError = "No simulated trajectory to export.";
        return;
    }
    try {
        exportBuildingTrajectoryCsv(plottedRun->raw, path);
        lastStatus = "Trajectory exported to: " + path;
    } catch (const std::exception& e) {
        lastError = e.what();
    }
}

} // namespace challenge_app::ui
