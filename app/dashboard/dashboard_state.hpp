#pragma once

#include "robot_simulation.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace robot_app::ui {

struct PlottedTrajectory {
    robot_app::Trajectory raw;
    std::vector<double> xs;
    std::vector<double> ys;
};

class DashboardState {
public:
    DashboardState();
    ~DashboardState();

    DashboardState(const DashboardState&) = delete;
    DashboardState& operator=(const DashboardState&) = delete;

    // Configuration editee dans les formulaires
    RobotConfig config;
    bool configDirty = true;
    bool resetPlotLimits = true;

    // Parametres de simulation en ligne
    double initX1 = 0.5;
    double initX2 = 4.5;
    double initX3 = 0.0;
    int numSimulations = 1;
    int maxSteps = 200;
    int randomSeed = 42;

    // Resultats de synthese et de simulation
    std::optional<SynthesisOutput> synthesis;
    RobotConfig synthesizedConfig;
    std::vector<PlottedTrajectory> trajectories;
    std::string lastError;
    std::string lastSimSummary;

    // Animation de la trajectoire active
    int selectedTrajectory = 0;
    int animationStep = 0;
    bool isPlaying = false;
    float playAccumulator = 0.0f;
    float stepsPerSecond = 8.0f;

    // Presets
    void loadFastTestPreset();
    void loadPaperExactPreset();

    // Synthese asynchrone (ne bloque jamais la fenetre)
    void startSynthesis();
    void pollSynthesis();
    bool isSynthesisRunning() const noexcept;
    int progressStep() const noexcept;
    int progressTotal() const noexcept;
    std::string progressMessage() const;

    // Simulation & Export
    void runSimulations();
    void exportSelectedCsv(const std::string& path);

private:
    void joinWorkerIfFinished();

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int> progressStep_{0};
    std::atomic<int> progressTotal_{5};

    mutable std::mutex mutex_;
    std::string progressMessage_ = "Pret.";
    bool hasPendingResult_ = false;
    std::optional<SynthesisOutput> pendingSynthesis_;
    RobotConfig pendingConfig_;
    std::string pendingError_;
};

} // namespace robot_app::ui