#include "dashboard_state.hpp"

#include <exception>
#include <utility>

namespace robot_app::ui {

DashboardState::DashboardState() {
    loadFastTestPreset();
}

DashboardState::~DashboardState() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void DashboardState::loadFastTestPreset() {
    config = makePaperDefaultConfig(false);
    // Regions elargies (2.0 x 2.0) adaptees a la grille rapide 20x20x12
    config.regions = {
        {"R1", 3.5, 5.5, 8.0, 10.0, {0.95f, 0.85f, 0.20f, 0.40f}},
        {"R2", 8.0, 10.0, 1.5, 3.5, {0.30f, 0.70f, 0.95f, 0.40f}},
        {"R3", 1.5, 3.5, 0.0, 2.0,  {0.45f, 0.85f, 0.35f, 0.40f}},
        {"R4", 4.0, 6.0, 4.0, 6.0,  {0.95f, 0.35f, 0.30f, 0.45f}}
    };
    initX1 = 0.5;
    initX2 = 4.5;
    initX3 = 0.0;
    numSimulations = 1;
    configDirty = true;
    resetPlotLimits = true;
    trajectories.clear();
    lastError.clear();
    lastSimSummary.clear();
}

void DashboardState::loadPaperExactPreset() {
    config = makePaperDefaultConfig(true); // Grille 100x100x30 par defaut
    initX1 = 0.5;
    initX2 = 4.5;
    initX3 = 0.0;
    numSimulations = 1;
    configDirty = true;
    resetPlotLimits = true;
    trajectories.clear();
    lastError.clear();
    lastSimSummary.clear();
}

void DashboardState::joinWorkerIfFinished() {
    if (!running_.load() && worker_.joinable()) {
        worker_.join();
    }
}

void DashboardState::startSynthesis() {
    if (running_.load()) {
        return;
    }
    joinWorkerIfFinished();

    lastError.clear();
    lastSimSummary.clear();
    trajectories.clear();
    isPlaying = false;
    animationStep = 0;

    const RobotConfig snapshot = config;
    running_.store(true);
    progressStep_.store(0);
    progressTotal_.store(5);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progressMessage_ = "Demarrage de la synthese...";
        hasPendingResult_ = false;
        pendingSynthesis_.reset();
        pendingError_.clear();
    }

    worker_ = std::thread([this, snapshot]() {
        try {
            SynthesisOutput output = runRobotSynthesis(
                snapshot, [this](int step, int total, const std::string& msg) {
                    progressStep_.store(step);
                    progressTotal_.store(total);
                    std::lock_guard<std::mutex> lock(mutex_);
                    progressMessage_ = msg;
                });

            std::lock_guard<std::mutex> lock(mutex_);
            pendingSynthesis_ = std::move(output);
            pendingConfig_ = snapshot;
            progressMessage_ = "Synthese terminee.";
            hasPendingResult_ = true;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingError_ = e.what();
            progressMessage_ = "Echec de la synthese.";
            hasPendingResult_ = true;
        }
        running_.store(false);
    });
}

void DashboardState::pollSynthesis() {
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
        synthesizedConfig = pendingConfig_;
        configDirty = false;
        lastError.clear();
    }
    pendingSynthesis_.reset();
    hasPendingResult_ = false;
}

bool DashboardState::isSynthesisRunning() const noexcept {
    return running_.load();
}

int DashboardState::progressStep() const noexcept {
    return progressStep_.load();
}

int DashboardState::progressTotal() const noexcept {
    return progressTotal_.load();
}

std::string DashboardState::progressMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progressMessage_;
}

void DashboardState::runSimulations() {
    lastError.clear();
    trajectories.clear();
    selectedTrajectory = 0;
    animationStep = 0;
    isPlaying = false;

    if (!synthesis.has_value()) {
        lastError = "Synthetisez d'abord un controleur avant de simuler.";
        return;
    }

    const int count = numSimulations < 1 ? 1 : numSimulations;
    const std::size_t horizon = maxSteps < 1 ? 1U : static_cast<std::size_t>(maxSteps);
    const symctrl::Vector x0{initX1, initX2, initX3};

    int successes = 0;
    for (int i = 0; i < count; ++i) {
        const auto seed = static_cast<std::uint32_t>(randomSeed + i);
        Trajectory traj = simulateClosedLoop(*synthesis, synthesizedConfig, x0, seed, horizon);
        if (traj.success) {
            ++successes;
        }

        PlottedTrajectory plotted;
        plotted.xs.reserve(traj.points.size());
        plotted.ys.reserve(traj.points.size());
        for (const auto& pt : traj.points) {
            plotted.xs.push_back(pt.x1);
            plotted.ys.push_back(pt.x2);
        }
        plotted.raw = std::move(traj);
        trajectories.push_back(std::move(plotted));
    }

    if (count == 1) {
        lastSimSummary = trajectories[0].raw.success
            ? ("SUCCES : " + trajectories[0].raw.statusMessage)
            : ("ECHEC : " + trajectories[0].raw.statusMessage);
    } else {
        lastSimSummary = std::to_string(successes) + " / " + std::to_string(count)
            + " trajectoires reussies.";
    }
}

void DashboardState::exportSelectedCsv(const std::string& path) {
    if (trajectories.empty()) {
        lastError = "Aucune trajectoire a exporter.";
        return;
    }
    const auto idx = static_cast<std::size_t>(selectedTrajectory);
    if (idx >= trajectories.size()) {
        return;
    }
    try {
        exportTrajectoryCsv(trajectories[idx].raw, path);
        lastSimSummary = "Trajectoire exportee dans : " + path;
    } catch (const std::exception& e) {
        lastError = e.what();
    }
}

} // namespace robot_app::ui