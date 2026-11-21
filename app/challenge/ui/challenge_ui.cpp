#include "challenge_ui.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace challenge_app::ui {
namespace {

std::string formatCount(bool fitsUint64, std::uint64_t exact, double approx) {
    char buf[64];
    if (fitsUint64) {
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(exact));
    } else if (std::isfinite(approx)) {
        std::snprintf(buf, sizeof(buf), "%.3e", approx);
    } else {
        std::snprintf(buf, sizeof(buf), "> 1e308");
    }
    return buf;
}

std::string formatRamMiB(double mib) {
    char buf[64];
    if (!std::isfinite(mib)) {
        return "> 1e300 MiB";
    }
    if (mib >= 1024.0 * 1024.0) {
        std::snprintf(buf, sizeof(buf), "%.2e GiB", mib / 1024.0);
    } else if (mib >= 1024.0) {
        std::snprintf(buf, sizeof(buf), "%.2f GiB", mib / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f MiB", mib);
    }
    return buf;
}

void renderParametersPanel(ChallengeUiState& state) {
    if (!ImGui::CollapsingHeader("1. Unknown Parameter N & Fixed Rules", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const bool running = state.isSynthesisRunning();
    bool changed = false;

    ImGui::BeginDisabled(running);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputInt("Rooms (N >= 3)", &state.roomsInput)) {
        if (state.roomsInput < 3) {
            state.roomsInput = 3;
        }
        state.syncInitialTemperaturesToN();
        changed = true;
    }

    const char* reachItems[] = {"Cooperative Monotone (corner bounds)", "Bounded Derivative (Jacobian)"};
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::Combo("Reachability", &state.reachabilityIndex, reachItems, 2)) {
        changed = true;
    }

    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputInt("RAM Budget (MiB)", &state.memoryBudgetMiB, 512, 2048)) {
        if (state.memoryBudgetMiB < 128) {
            state.memoryBudgetMiB = 128;
        }
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextDisabled("Fixed Challenge Specification (Slides 14-16):");
    ImGui::BulletText("X = S = [%.0f, %.0f]^%d, nd = %u (eta = 0.5 C)",
                      kTempMin, kTempMax, state.roomsInput, kCellsPerDimension);
    ImGui::BulletText("U = {0, 0.3, 0.6}^%d (nu = 3), W = empty", state.roomsInput);
    ImGui::BulletText("alpha=%.2f, beta=%.3f, gamma=%.2f, Te=%.0f, Th=%.0f",
                      kAlpha, kBeta, kGamma, kOutsideTemp, kHeaterTemp);

    const ScaleEstimate est = estimateChallengeScale(static_cast<std::size_t>(state.roomsInput));
    ImGui::Spacing();
    ImGui::TextDisabled("Pre-flight Scale Estimate for N = %d:", state.roomsInput);
    const std::string statesStr = formatCount(est.statesFitUint64, est.symbolicStates, est.statesApprox);
    const std::string inputsStr = formatCount(est.inputsFitUint64, est.symbolicInputs, est.inputsApprox);
    const std::string pairsStr = formatCount(est.pairsFitUint64, est.stateInputPairs, est.pairsApprox);
    const std::string ramStr = formatRamMiB(est.estimatedRamMiB);

    ImGui::Text("Symbolic states (10^N): %s", statesStr.c_str());
    ImGui::Text("Symbolic inputs (3^N) : %s", inputsStr.c_str());
    ImGui::Text("State-input pairs     : %s", pairsStr.c_str());
    if (est.estimatedRamMiB > static_cast<double>(state.memoryBudgetMiB)) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.30f, 1.0f),
                           "Est. explicit RAM     : %s (exceeds budget)", ramStr.c_str());
    } else {
        ImGui::Text("Est. explicit RAM     : %s", ramStr.c_str());
    }

    if (changed) {
        state.configDirty = true;
    }
}

void renderSynthesisPanel(ChallengeUiState& state) {
    if (!ImGui::CollapsingHeader("2. Safety Synthesis & Competition Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const bool running = state.isSynthesisRunning();
    ImGui::BeginDisabled(running);
    if (ImGui::Button("Synthesize Safety Controller", ImVec2(-1.0f, 34.0f))) {
        state.startSynthesis();
    }
    ImGui::EndDisabled();

    if (running) {
        const int step = state.progressStep();
        const int total = std::max(1, state.progressTotal());
        const float frac = static_cast<float>(step) / static_cast<float>(total);
        const std::string msg = state.progressMessage();
        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), msg.c_str());
    } else if (state.synthesis.has_value()) {
        const auto& m = state.synthesis->metrics;
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
                           "Ready for N = %zu (%s)",
                           m.rooms, symctrl::toString(m.numericGuarantee));
        ImGui::Text("T_abs (abstraction) : %.4f s", m.abstractionSeconds);
        ImGui::Text("T_ctrl (synthesis)  : %.4f s", m.synthesisSeconds);
        ImGui::Text("Total (T_abs+T_ctrl): %.4f s", m.totalSeconds);
        ImGui::Text("Controllable states : %llu / %llu (%.2f%%)",
                    static_cast<unsigned long long>(m.controllableStates),
                    static_cast<unsigned long long>(m.internalStates),
                    m.controllablePercent);
        ImGui::Text("Stored edges        : %llu",
                    static_cast<unsigned long long>(m.storedEdges));
        if (m.memoryMeasured) {
            ImGui::Text("Peak RSS / Current  : %.2f MiB / %.2f MiB (%s)",
                        m.peakMemoryMiB, m.currentMemoryMiB,
                        m.isolatedProcess ? "isolated process" : "in-process");
        } else {
            ImGui::Text("Peak RSS            : unavailable");
        }
        if (state.configDirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f),
                               "Parameters modified: re-run synthesis.");
        }
    }

    if (!state.lastError.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Error: %s", state.lastError.c_str());
    }
}

void renderSimulationPanel(ChallengeUiState& state) {
    if (!ImGui::CollapsingHeader("3. Closed-Loop Simulation (T(0) in [17, 22]^N)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    state.syncInitialTemperaturesToN();

    if (ImGui::SmallButton("Cold 17.2 C")) {
        state.applyUniformInitial(17.2);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Hot 21.8 C")) {
        state.applyUniformInitial(21.8);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Alternating 17.5/21.5")) {
        state.applyAlternatingInitial(17.5, 21.5);
    }

    const float listHeight = std::min(150.0f, 26.0f * static_cast<float>(state.initialTemperatures.size()) + 10.0f);
    ImGui::BeginChild("InitialTempList", ImVec2(0.0f, listHeight), true);
    for (std::size_t i = 0; i < state.initialTemperatures.size(); ++i) {
        const std::string label = "Room " + std::to_string(i + 1) + " T0 (C)";
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputDouble(label.c_str(), &state.initialTemperatures[i], 0.1, 0.5, "%.2f");
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::InputInt("Horizon (steps)", &state.horizon)) {
        if (state.horizon < 1) {
            state.horizon = 1;
        }
    }
    const char* stratItems[] = {"Min Heater Energy", "First Safe Action", "Max Heating"};
    ImGui::SetNextItemWidth(180.0f);
    ImGui::Combo("Safe Policy Choice", &state.strategyIndex, stratItems, 3);

    const bool canSimulate = state.synthesis.has_value()
        && !state.isSynthesisRunning()
        && state.synthesis->metrics.rooms == static_cast<std::size_t>(state.roomsInput);

    ImGui::BeginDisabled(!canSimulate);
    if (ImGui::Button("Run Closed-Loop Simulation", ImVec2(-1.0f, 30.0f))) {
        state.runSimulation();
    }
    ImGui::EndDisabled();

    if (state.plottedRun.has_value()) {
        if (ImGui::Button("Export Trajectory CSV")) {
            const std::string filename = "challenge_n"
                + std::to_string(state.synthesis->metrics.rooms) + "_trajectory.csv";
            state.exportCsv(filename);
        }
    }

    if (!state.lastStatus.empty()) {
        ImGui::TextWrapped("%s", state.lastStatus.c_str());
    }
}

void renderHistoryTable(const ChallengeUiState& state) {
    if (state.history.empty()) {
        return;
    }
    if (!ImGui::CollapsingHeader("Benchmark History (Session Runs)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("HistoryTable", 6, flags)) {
        ImGui::TableSetupColumn("N");
        ImGui::TableSetupColumn("10^N");
        ImGui::TableSetupColumn("T_abs (s)");
        ImGui::TableSetupColumn("T_ctrl (s)");
        ImGui::TableSetupColumn("Peak (MiB)");
        ImGui::TableSetupColumn("Safe %");
        ImGui::TableHeadersRow();

        for (const auto& m : state.history) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%zu", m.rooms);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(m.internalStates));
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", m.abstractionSeconds);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", m.synthesisSeconds);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", m.peakMemoryMiB);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f%%", m.controllablePercent);
        }
        ImGui::EndTable();
    }
}

void renderPlotsPanel(const ChallengeUiState& state) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float topHeight = std::max(200.0f, avail.y * 0.58f);
    const float bottomHeight = std::max(160.0f, avail.y - topHeight - 10.0f);

    if (ImPlot::BeginPlot("Room Temperatures T_i(t) vs Comfort Zone [17, 22] C", ImVec2(-1.0f, topHeight))) {
        ImPlot::SetupAxes("Time step t", "Temperature (C)");
        const double maxT = state.plottedRun.has_value() && !state.plottedRun->timeSteps.empty()
            ? state.plottedRun->timeSteps.back()
            : static_cast<double>(state.horizon);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, std::max(1.0, maxT), ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, kTempMin - 0.8, kTempMax + 0.8, ImPlotCond_Always);

        const double boundX[2] = {0.0, std::max(1.0, maxT)};
        const double lowerY[2] = {kTempMin, kTempMin};
        const double upperY[2] = {kTempMax, kTempMax};
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.35f, 0.35f, 0.9f), 2.0f);
        ImPlot::PlotLine("Safety Bounds [17, 22]", boundX, lowerY, 2);
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.35f, 0.35f, 0.9f), 2.0f);
        ImPlot::PlotLine("##upperBound", boundX, upperY, 2);

        if (state.plottedRun.has_value()) {
            const auto& run = *state.plottedRun;
            const int count = static_cast<int>(run.timeSteps.size());
            for (std::size_t r = 0; r < run.roomTemperatures.size(); ++r) {
                const std::string label = "Room " + std::to_string(r + 1);
                ImPlot::PlotLine(label.c_str(), run.timeSteps.data(), run.roomTemperatures[r].data(), count);
            }
        }
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Applied Heater Inputs u_i(t) in {0, 0.3, 0.6}", ImVec2(-1.0f, bottomHeight))) {
        ImPlot::SetupAxes("Time step t", "Heater Control u_i");
        const double maxT = state.plottedRun.has_value() && !state.plottedRun->timeSteps.empty()
            ? state.plottedRun->timeSteps.back()
            : static_cast<double>(state.horizon);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, std::max(1.0, maxT), ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.05, 0.70, ImPlotCond_Always);

        if (state.plottedRun.has_value() && !state.plottedRun->inputTimeSteps.empty()) {
            const auto& run = *state.plottedRun;
            const int count = static_cast<int>(run.inputTimeSteps.size());
            for (std::size_t r = 0; r < run.roomInputs.size(); ++r) {
                const std::string label = "u_" + std::to_string(r + 1);
                ImPlot::PlotStairs(label.c_str(), run.inputTimeSteps.data(), run.roomInputs[r].data(), count);
            }
        }
        ImPlot::EndPlot();
    }
}

} // namespace

void renderChallengeUi(ChallengeUiState& state) {
    state.pollSynthesis();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("SymCtrlChallengeRoot", nullptr, rootFlags);

    ImGui::BeginChild("LeftPanel", ImVec2(460.0f, 0.0f), true);
    ImGui::Text("Circular Building Temperature Regulation Challenge");
    ImGui::Separator();
    renderParametersPanel(state);
    renderSynthesisPanel(state);
    renderSimulationPanel(state);
    renderHistoryTable(state);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(0.0f, 0.0f), false);
    renderPlotsPanel(state);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace challenge_app::ui
