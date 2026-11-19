#include "mission_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace robot_app {
namespace {

std::string trim(const std::string& raw) {
    std::size_t start = 0;
    while (start < raw.size() && std::isspace(static_cast<unsigned char>(raw[start])) != 0) {
        ++start;
    }
    std::size_t end = raw.size();
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1])) != 0) {
        --end;
    }
    return raw.substr(start, end - start);
}

bool containsLabel(const std::vector<symctrl::part2::LabelId>& list,
                   symctrl::part2::LabelId label) {
    return std::find(list.begin(), list.end(), label) != list.end();
}

std::vector<symctrl::part2::LabelId> resolveNames(
    const std::vector<std::string>& names,
    const std::unordered_map<std::string, symctrl::part2::LabelId>& nameToLabel,
    const char* fieldName) {
    std::vector<symctrl::part2::LabelId> result;
    for (const std::string& raw : names) {
        const std::string name = trim(raw);
        if (name.empty()) {
            continue;
        }
        const auto it = nameToLabel.find(name);
        if (it == nameToLabel.end()) {
            throw std::invalid_argument(std::string(fieldName) + ": region inconnue '" + name + "'");
        }
        if (!containsLabel(result, it->second)) {
            result.push_back(it->second);
        }
    }
    return result;
}

bool isCellInsideRegion2D(const symctrl::Box& cell, const RegionSpec& region, double eps = 1e-12) {
    return cell.lo[0] >= region.x1Min - eps && cell.hi[0] <= region.x1Max + eps
        && cell.lo[1] >= region.x2Min - eps && cell.hi[1] <= region.x2Max + eps;
}

} // namespace

symctrl::part2::Automaton compileMissionToAutomaton(const std::vector<RegionSpec>& regions,
                                                    const MissionSpec& mission) {
    using namespace symctrl::part2;

    if (regions.empty()) {
        throw std::invalid_argument("Aucune region definie.");
    }

    std::unordered_map<std::string, LabelId> nameToLabel;
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const std::string name = trim(regions[i].name);
        if (name.empty()) {
            throw std::invalid_argument("Une region a un nom vide.");
        }
        if (!std::isfinite(regions[i].x1Min) || !std::isfinite(regions[i].x1Max)
            || !std::isfinite(regions[i].x2Min) || !std::isfinite(regions[i].x2Max)
            || !(regions[i].x1Min < regions[i].x1Max) || !(regions[i].x2Min < regions[i].x2Max)) {
            throw std::invalid_argument("Coordonnees invalides pour la region '" + name + "'");
        }
        const LabelId label = static_cast<LabelId>(i + 1); // 0 = zone neutre
        if (!nameToLabel.emplace(name, label).second) {
            throw std::invalid_argument("Nom de region en double : '" + name + "'");
        }
    }

    const auto visitFirst = resolveNames(mission.visitFirst, nameToLabel, "Visit first");
    const auto thenGoTo = resolveNames(mission.thenGoTo, nameToLabel, "Then go to");
    const auto avoid = resolveNames(mission.avoid, nameToLabel, "Avoid");

    if (visitFirst.empty()) {
        throw std::invalid_argument("Le champ 'Visit first' doit contenir au moins une region.");
    }

    for (LabelId l : visitFirst) {
        if (containsLabel(avoid, l) || containsLabel(thenGoTo, l)) {
            throw std::invalid_argument("Une meme region apparait dans deux categories.");
        }
    }
    for (LabelId l : thenGoTo) {
        if (containsLabel(avoid, l)) {
            throw std::invalid_argument("Une region de 'Then go to' ne peut pas etre dans 'Avoid'.");
        }
    }

    const std::size_t totalLabels = regions.size() + 1;
    Automaton automaton;
    automaton.initialState = 0;

    // Cas 1 : pas de "Then go to" -> atteignabilite + evitement
    if (thenGoTo.empty()) {
        constexpr AutomatonStateId kInit = 0;
        constexpr AutomatonStateId kGoal = 1;
        constexpr AutomatonStateId kSink = 2;
        automaton.finalStates = {kGoal};
        automaton.transitions.assign(3, std::vector<AutomatonStateId>(totalLabels));

        for (std::size_t idx = 0; idx < totalLabels; ++idx) {
            const LabelId l = static_cast<LabelId>(idx);
            if (containsLabel(avoid, l)) {
                automaton.transitions[kInit][idx] = kSink;
            } else if (containsLabel(visitFirst, l)) {
                automaton.transitions[kInit][idx] = kGoal;
            } else {
                automaton.transitions[kInit][idx] = kInit;
            }
            automaton.transitions[kGoal][idx] = kGoal;
            automaton.transitions[kSink][idx] = kSink;
        }
        validateAutomaton(automaton);
        return automaton;
    }

    // Cas 2 : "Visit first" -> "Then go to" (+ "Avoid")
    const std::size_t branchCount = mission.visitFirstExclusive ? visitFirst.size() : 1;
    const AutomatonStateId kInit = 0;
    const AutomatonStateId kGoal = static_cast<AutomatonStateId>(1 + branchCount);
    const AutomatonStateId kSink = static_cast<AutomatonStateId>(2 + branchCount);
    const std::size_t stateCount = branchCount + 3;

    automaton.finalStates = {kGoal};
    automaton.transitions.assign(stateCount, std::vector<AutomatonStateId>(totalLabels));

    for (std::size_t idx = 0; idx < totalLabels; ++idx) {
        const LabelId l = static_cast<LabelId>(idx);
        if (containsLabel(avoid, l)) {
            automaton.transitions[kInit][idx] = kSink;
        } else {
            const auto it = std::find(visitFirst.begin(), visitFirst.end(), l);
            if (it != visitFirst.end()) {
                const std::size_t pos = static_cast<std::size_t>(it - visitFirst.begin());
                automaton.transitions[kInit][idx] = mission.visitFirstExclusive
                    ? static_cast<AutomatonStateId>(1 + pos)
                    : AutomatonStateId{1};
            } else {
                automaton.transitions[kInit][idx] = kInit;
            }
        }
    }

    for (std::size_t branch = 0; branch < branchCount; ++branch) {
        const AutomatonStateId q = static_cast<AutomatonStateId>(1 + branch);
        for (std::size_t idx = 0; idx < totalLabels; ++idx) {
            const LabelId l = static_cast<LabelId>(idx);
            if (containsLabel(avoid, l)) {
                automaton.transitions[q][idx] = kSink;
            } else if (mission.visitFirstExclusive
                       && containsLabel(visitFirst, l)
                       && l != visitFirst[branch]) {
                automaton.transitions[q][idx] = kSink;
            } else if (containsLabel(thenGoTo, l)) {
                automaton.transitions[q][idx] = kGoal;
            } else {
                automaton.transitions[q][idx] = q;
            }
        }
    }

    for (std::size_t idx = 0; idx < totalLabels; ++idx) {
        automaton.transitions[kGoal][idx] = kGoal;
        automaton.transitions[kSink][idx] = kSink;
    }

    validateAutomaton(automaton);
    return automaton;
}

symctrl::part2::StateLabels buildRegionLabels(const symctrl::IStateGrid& grid,
                                              const std::vector<RegionSpec>& regions) {
    symctrl::part2::StateLabels labels;
    const symctrl::StateId count = grid.internalStateCount();

    for (symctrl::StateId state = 1; state <= count; ++state) {
        const symctrl::Box cell = grid.cellBounds(state);
        symctrl::part2::LabelId assigned = 0;
        for (std::size_t i = 0; i < regions.size(); ++i) {
            if (isCellInsideRegion2D(cell, regions[i])) {
                assigned = static_cast<symctrl::part2::LabelId>(i + 1);
                break;
            }
        }
        labels[state] = assigned;
    }
    return labels;
}

} // namespace robot_app