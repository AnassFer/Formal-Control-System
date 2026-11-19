#include "dashboard_ui.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace robot_app::ui {
namespace {

bool inputTextString(const char* id, std::string& value, float width) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText(id, buffer, sizeof(buffer))) {
        value = buffer;
        return true;
    }
    return false;
}

bool inputPositiveUint32(const char* label, std::uint32_t& value, std::uint32_t minVal = 1) {
    int tmp = static_cast<int>(value);
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::InputInt(label, &tmp)) {
        if (tmp < static_cast<int>(minVal)) {
            tmp = static_cast<int>(minVal);
        }
        value = static_cast<std::uint32_t>(tmp);
        return true;
    }
    return false;
}

bool inputPositiveSizeT(const char* label, std::size_t& value, std::size_t minVal = 1) {
    int tmp = static_cast<int>(value);
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::InputInt(label, &tmp)) {
        if (tmp < static_cast<int>(minVal)) {
            tmp = static_cast<int>(minVal);
        }
        value = static_cast<std::size_t>(tmp);
        return true;
    }
    return false;
}

bool drawDynamicRegionRow(const char* title, std::vector<std::string>& entries) {
    bool changed = false;
    if (entries.empty()) {
        entries.emplace_back("");
    }

    ImGui::PushID(title);
    ImGui::Text("%-12s", title);

    int removeIndex = -1;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        ImGui::SameLine();
        if (i > 0) {
            ImGui::TextDisabled("OU");
            ImGui::SameLine();
        }

        ImGui::PushID(static_cast<int>(i));
        if (inputTextString("##box", entries[i], 68.0f)) {
            changed = true;
        }
        if (entries.size() > 1) {
            ImGui::SameLine(0.0f, 3.0f);
            if (ImGui::SmallButton("x")) {
                removeIndex = static_cast<int>(i);
            }
        }
        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        entries.erase(entries.begin() + removeIndex);
        changed = true;
    }

    // Le bouton "+ OU" apparait uniquement lorsqu'un nom de boite est saisi
    if (!entries.back().empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("+ OU")) {
            entries.emplace_back("");
            changed = true;
        }
    }

    ImGui::PopID();
    return changed;
}

std::string buildMissionPreview(const MissionSpec& mission) {
    auto joinList = [](const std::vector<std::string>& list, const char* sep) {
        std::string out;
        for (const auto& item : list) {
            if (item.empty()) {
                continue;
            }
            if (!out.empty()) {
                out += sep;
            }
            out += item;
        }
        return out;
    };

    const std::string first = joinList(mission.visitFirst,
                                       mission.visitFirstExclusive ? " XOR " : " OU ");
    const std::string thenStr = joinList(mission.thenGoTo, " OU ");
    const std::string avoidStr = joinList(mission.avoid, ", ");

    std::string preview = first.empty() ? "(aucune cible)" : "(" + first + ")";
    if (!thenStr.empty()) {
        preview += "  ->  (" + thenStr + ")";
    }
    if (!avoidStr.empty()) {
        preview += "  |  Eviter {" + avoidStr + "}";
    }
    return preview;
}

void renderDomainSection(DashboardState& state) {
    if (!ImGui::CollapsingHeader("1. Domaine (X, U, W) et Discretisation", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    bool changed = false;
    auto& cfg = state.config;

    ImGui::TextDisabled("Domaine d'etat X");
    ImGui::PushItemWidth(85.0f);
    changed |= ImGui::InputDouble("x1 min##x1", &cfg.x1Min, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("x1 max##x1", &cfg.x1Max, 0.0, 0.0, "%.2f");
    changed |= ImGui::InputDouble("x2 min##x2", &cfg.x2Min, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("x2 max##x2", &cfg.x2Max, 0.0, 0.0, "%.2f");
    changed |= ImGui::InputDouble("x3 min##x3", &cfg.x3Min, 0.0, 0.0, "%.3f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("x3 max##x3", &cfg.x3Max, 0.0, 0.0, "%.3f");
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::TextDisabled("Cellules de la grille (Nx1, Nx2, Nx3)");
    changed |= inputPositiveUint32("Nx1", cfg.nx1, 2);
    ImGui::SameLine();
    changed |= inputPositiveUint32("Nx2", cfg.nx2, 2);
    ImGui::SameLine();
    changed |= inputPositiveUint32("Nx3", cfg.nx3, 2);

    const std::uint64_t totalStates =
        static_cast<std::uint64_t>(cfg.nx1) * cfg.nx2 * cfg.nx3;

    ImGui::Spacing();
    ImGui::TextDisabled("Commandes U et Discretisation");
    ImGui::PushItemWidth(85.0f);
    changed |= ImGui::InputDouble("u1 min", &cfg.u1Min, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("u1 max", &cfg.u1Max, 0.0, 0.0, "%.2f");
    changed |= ImGui::InputDouble("u2 min", &cfg.u2Min, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("u2 max", &cfg.u2Max, 0.0, 0.0, "%.2f");
    ImGui::PopItemWidth();

    changed |= inputPositiveSizeT("Nu1", cfg.nu1, 1);
    ImGui::SameLine();
    changed |= inputPositiveSizeT("Nu2", cfg.nu2, 1);

    ImGui::Spacing();
    ImGui::TextDisabled("Perturbations [-w, +w] et Pas d'echantillonnage");
    ImGui::PushItemWidth(75.0f);
    changed |= ImGui::InputDouble("w1", &cfg.w1Max, 0.0, 0.0, "%.3f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("w2", &cfg.w2Max, 0.0, 0.0, "%.3f");
    ImGui::SameLine();
    changed |= ImGui::InputDouble("w3", &cfg.w3Max, 0.0, 0.0, "%.3f");
    changed |= ImGui::InputDouble("tau (s)", &cfg.tau, 0.0, 0.0, "%.2f");
    ImGui::PopItemWidth();

    const std::uint64_t totalInputs =
        static_cast<std::uint64_t>(cfg.nu1) * cfg.nu2;
    ImGui::Text("Resume : %llu etats internes | %llu commandes",
                static_cast<unsigned long long>(totalStates),
                static_cast<unsigned long long>(totalInputs));

    if (changed) {
        state.configDirty = true;
    }
}

void renderRegionsSection(DashboardState& state) {
    if (!ImGui::CollapsingHeader("2. Regions (R1, R2, ...)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto& regions = state.config.regions;
    int removeIndex = -1;
    bool changed = false;

    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("RegionsTable", 7, flags)) {
        ImGui::TableSetupColumn("Nom", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("x1 min");
        ImGui::TableSetupColumn("x1 max");
        ImGui::TableSetupColumn("x2 min");
        ImGui::TableSetupColumn("x2 max");
        ImGui::TableSetupColumn("Coul", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < regions.size(); ++i) {
            auto& box = regions[i];
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            ImGui::TableNextColumn();
            changed |= inputTextString("##name", box.name, -FLT_MIN);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::InputDouble("##x1min", &box.x1Min, 0.0, 0.0, "%.2f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::InputDouble("##x1max", &box.x1Max, 0.0, 0.0, "%.2f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::InputDouble("##x2min", &box.x2Min, 0.0, 0.0, "%.2f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed |= ImGui::InputDouble("##x2max", &box.x2Max, 0.0, 0.0, "%.2f");

            ImGui::TableNextColumn();
            ImGui::ColorEdit4("##color", box.color.data(), ImGuiColorEditFlags_NoInputs);

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) {
                removeIndex = static_cast<int>(i);
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (removeIndex >= 0) {
        regions.erase(regions.begin() + removeIndex);
        changed = true;
    }

    if (ImGui::Button("+ Ajouter une region")) {
        const std::string nextName = "R" + std::to_string(regions.size() + 1);
        regions.push_back({nextName, 0.0, 1.5, 0.0, 1.5, {0.65f, 0.45f, 0.90f, 0.40f}});
        changed = true;
    }

    if (changed) {
        state.configDirty = true;
    }
}

void renderMissionSection(DashboardState& state) {
    if (!ImGui::CollapsingHeader("3. Specification de la mission", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto& mission = state.config.mission;
    bool changed = false;

    changed |= drawDynamicRegionRow("Visit first", mission.visitFirst);
    if (mission.visitFirst.size() > 1) {
        changed |= ImGui::Checkbox("Un seul parmi 'Visit first' (XOR)", &mission.visitFirstExclusive);
    }
    changed |= drawDynamicRegionRow("Then go to", mission.thenGoTo);
    changed |= drawDynamicRegionRow("Avoid", mission.avoid);

    ImGui::Spacing();
    const std::string preview = buildMissionPreview(mission);
    ImGui::TextWrapped("Automate : %s", preview.c_str());

    if (changed) {
        state.configDirty = true;
    }
}

void renderSynthesisControls(DashboardState& state) {
    ImGui::Separator();
    const bool running = state.isSynthesisRunning();

    ImGui::BeginDisabled(running);
    if (ImGui::Button("Synthetiser le controleur", ImVec2(-1.0f, 34.0f))) {
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
        const auto& s = *state.synthesis;
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
                           "Controleur pret (%.2f s | aretes: %llu)",
                           s.totalSeconds,
                           static_cast<unsigned long long>(s.storedEdges));
        ImGui::Text("Domaine gagnant |Q0| : %zu / %llu",
                    s.winningInitialStates.size(),
                    static_cast<unsigned long long>(s.grid->internalStateCount()));
        if (state.configDirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                               "Parametres modifies : relancez la synthese.");
        }
    }

    if (!state.lastError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           "Erreur : %s", state.lastError.c_str());
    }
}

void renderSimulationBar(DashboardState& state, float deltaSeconds) {
    ImGui::Text("Condition initiale x0 (Maj+Clic sur le graphe pour placer) :");
    ImGui::PushItemWidth(80.0f);
    ImGui::InputDouble("x1##init", &state.initX1, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    ImGui::InputDouble("x2##init", &state.initX2, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    ImGui::InputDouble("x3##init", &state.initX3, 0.0, 0.0, "%.2f");
    ImGui::SameLine();
    ImGui::InputInt("Nb traj.", &state.numSimulations);
    if (state.numSimulations < 1) {
        state.numSimulations = 1;
    }
    ImGui::SameLine();
    ImGui::InputInt("Seed", &state.randomSeed);
    ImGui::PopItemWidth();

    const bool canSimulate = state.synthesis.has_value() && !state.isSynthesisRunning();
    ImGui::BeginDisabled(!canSimulate);
    if (ImGui::Button("Simulation", ImVec2(140.0f, 28.0f))) {
        state.runSimulations();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Recadrer la vue")) {
        state.resetPlotLimits = true;
    }

    if (!state.trajectories.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Exporter CSV")) {
            state.exportSelectedCsv("simulation_dashboard.csv");
        }

        const auto& active = state.trajectories[static_cast<std::size_t>(state.selectedTrajectory)];
        const int maxIdx = active.raw.points.empty()
            ? 0
            : static_cast<int>(active.raw.points.size() - 1);

        if (state.isPlaying && maxIdx > 0) {
            state.playAccumulator += deltaSeconds * state.stepsPerSecond;
            while (state.playAccumulator >= 1.0f) {
                state.playAccumulator -= 1.0f;
                if (state.animationStep < maxIdx) {
                    ++state.animationStep;
                } else {
                    state.isPlaying = false;
                    break;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(state.isPlaying ? "Pause" : "Animer")) {
            if (!state.isPlaying && state.animationStep >= maxIdx) {
                state.animationStep = 0;
            }
            state.isPlaying = !state.isPlaying;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderInt("Pas t", &state.animationStep, 0, maxIdx);
        state.animationStep = std::clamp(state.animationStep, 0, maxIdx);
    }

    if (!state.lastSimSummary.empty()) {
        ImGui::Text("%s", state.lastSimSummary.c_str());
    }
}

void renderPlotCanvas(DashboardState& state) {
    const auto& cfg = state.config;
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    if (ImPlot::BeginPlot("Simulation du robot mobile (x1, x2)", avail, ImPlotFlags_Equal)) {
        ImPlot::SetupAxes("x1", "x2");
        const ImPlotCond cond = state.resetPlotLimits ? ImPlotCond_Always : ImPlotCond_Once;
        const double padX = 0.05 * (cfg.x1Max - cfg.x1Min);
        const double padY = 0.05 * (cfg.x2Max - cfg.x2Min);
        ImPlot::SetupAxisLimits(ImAxis_X1, cfg.x1Min - padX, cfg.x1Max + padX, cond);
        ImPlot::SetupAxisLimits(ImAxis_Y1, cfg.x2Min - padY, cfg.x2Max + padY, cond);
        state.resetPlotLimits = false;

        ImDrawList* drawList = ImPlot::GetPlotDrawList();
        ImPlot::PushPlotClipRect();

        // Cadre du domaine X
        const ImVec2 domMin = ImPlot::PlotToPixels(ImPlotPoint(cfg.x1Min, cfg.x2Max));
        const ImVec2 domMax = ImPlot::PlotToPixels(ImPlotPoint(cfg.x1Max, cfg.x2Min));
        drawList->AddRect(domMin, domMax, IM_COL32(220, 220, 220, 200), 0.0f, 0, 2.0f);

        // Regions R1, R2, ...
        for (const auto& reg : cfg.regions) {
            const ImVec2 pMin = ImPlot::PlotToPixels(ImPlotPoint(reg.x1Min, reg.x2Max));
            const ImVec2 pMax = ImPlot::PlotToPixels(ImPlotPoint(reg.x1Max, reg.x2Min));
            const ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(reg.color[0], reg.color[1], reg.color[2], reg.color[3]));
            const ImU32 edgeCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(reg.color[0], reg.color[1], reg.color[2], 0.95f));
            drawList->AddRectFilled(pMin, pMax, fillCol);
            drawList->AddRect(pMin, pMax, edgeCol, 0.0f, 0, 2.0f);
        }
        ImPlot::PopPlotClipRect();

        for (const auto& reg : cfg.regions) {
            if (!reg.name.empty()) {
                ImPlot::PlotText(reg.name.c_str(),
                                 0.5 * (reg.x1Min + reg.x1Max),
                                 0.5 * (reg.x2Min + reg.x2Max));
            }
        }

        // Trajectoires simulees
        for (std::size_t i = 0; i < state.trajectories.size(); ++i) {
            const auto& traj = state.trajectories[i];
            if (traj.xs.empty()) {
                continue;
            }
            const std::string label = (i == 0) ? "Trajectoires" : ("##traj" + std::to_string(i));
            const ImVec4 col = traj.raw.success
                ? ImVec4(0.15f, 0.85f, 0.95f, state.trajectories.size() > 1 ? 0.55f : 1.0f)
                : ImVec4(1.00f, 0.35f, 0.35f, 0.85f);
            ImPlot::SetNextLineStyle(col, state.trajectories.size() > 1 ? 1.5f : 2.5f);
            ImPlot::PlotLine(label.c_str(), traj.xs.data(), traj.ys.data(),
                             static_cast<int>(traj.xs.size()));
        }

        // Marqueur du point de depart x0
        const double x0Arr[1] = {state.initX1};
        const double y0Arr[1] = {state.initX2};
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6.0f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f);
        ImPlot::PlotScatter("Depart x0", x0Arr, y0Arr, 1);

        // Position courante du robot + fleche d'orientation x3 pendant l'animation
        if (!state.trajectories.empty()) {
            const auto& active = state.trajectories[static_cast<std::size_t>(state.selectedTrajectory)];
            if (!active.raw.points.empty()) {
                const auto idx = static_cast<std::size_t>(
                    std::clamp(state.animationStep, 0, static_cast<int>(active.raw.points.size() - 1)));
                const auto& pt = active.raw.points[idx];
                const double arrowLen = 0.04 * (cfg.x1Max - cfg.x1Min);

                ImPlot::PushPlotClipRect();
                const ImVec2 center = ImPlot::PlotToPixels(ImPlotPoint(pt.x1, pt.x2));
                const ImVec2 tip = ImPlot::PlotToPixels(
                    ImPlotPoint(pt.x1 + arrowLen * std::cos(pt.x3),
                                pt.x2 + arrowLen * std::sin(pt.x3)));
                drawList->AddCircleFilled(center, 6.0f, IM_COL32(255, 220, 40, 255));
                drawList->AddLine(center, tip, IM_COL32(255, 80, 40, 255), 3.0f);
                ImPlot::PopPlotClipRect();
            }
        }

        // Maj + Clic gauche sur le graphe pour deplacer x0
        if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && ImGui::GetIO().KeyShift) {
            const ImPlotPoint p = ImPlot::GetPlotMousePos();
            state.initX1 = p.x;
            state.initX2 = p.y;
        }

        ImPlot::EndPlot();
    }
}

} // namespace

void renderDashboard(DashboardState& state, float deltaSeconds) {
    state.pollSynthesis();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("SymCtrlDashboardRoot", nullptr, rootFlags);

    // Colonne gauche : parametres, tableau des regions, specification dynamique
    const float leftWidth = 490.0f;
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0.0f), true);

    ImGui::Text("Presets :");
    ImGui::SameLine();
    if (ImGui::Button("Test rapide (20x20x12)")) {
        state.loadFastTestPreset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Papier (boites exactes)")) {
        state.loadPaperExactPreset();
    }

    ImGui::Separator();
    renderDomainSection(state);
    renderRegionsSection(state);
    renderMissionSection(state);
    renderSynthesisControls(state);

    ImGui::EndChild();

    ImGui::SameLine();

    // Colonne droite : barre de simulation + graphe 2D ImPlot
    ImGui::BeginChild("RightPanel", ImVec2(0.0f, 0.0f), false);
    renderSimulationBar(state, deltaSeconds);
    ImGui::Separator();
    renderPlotCanvas(state);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace robot_app::ui