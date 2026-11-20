#include "mission_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <queue>

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

bool isCellIntersectingRegion2D(const symctrl::Box& cell, const RegionSpec& region, double eps = 1e-12) {
    return cell.hi[0] > region.x1Min + eps && cell.lo[0] < region.x1Max - eps
        && cell.hi[1] > region.x2Min + eps && cell.lo[1] < region.x2Max - eps;
}

} // namespace

symctrl::part2::Automaton compileMissionToAutomaton(const std::vector<RegionSpec>& regions,
                                                    const MissionSpec& mission) {
    using namespace symctrl::part2;

    if (regions.empty()) {
        throw std::invalid_argument("Aucune region definie.");
    }
    if (regions.size() >= 63) {
        throw std::invalid_argument("Nombre maximal de regions depasse (62 max).");
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

    const auto avoid = resolveNames(mission.avoid, nameToLabel, "Avoid");

    std::uint64_t allAvoidMask = 0ULL;
    for (LabelId l : avoid) {
        allAvoidMask |= (1ULL << l);
    }
    const bool useAvoidOr = mission.avoidOr && avoid.size() > 1;

    struct ResolvedStage {
        std::vector<LabelId> labels;
        bool exclusive = false;
    };
    std::vector<ResolvedStage> stages;
    for (std::size_t s = 0; s < mission.stages.size(); ++s) {
        const std::string field = "Etape " + std::to_string(s + 1);
        auto labels = resolveNames(mission.stages[s].regions, nameToLabel, field.c_str());
        if (labels.empty()) {
            continue;
        }
        for (LabelId l : labels) {
            if (containsLabel(avoid, l)) {
                throw std::invalid_argument(field + ": une region cible ne peut pas etre dans 'Avoid'.");
            }
        }
        stages.push_back({std::move(labels), mission.stages[s].exclusive && labels.size() > 1});
    }

    if (stages.empty()) {
        throw std::invalid_argument("La mission doit contenir au moins une etape non vide.");
    }

    const std::size_t totalLabels = regions.size() + 1;
    constexpr AutomatonStateId kInit = 0;
    constexpr AutomatonStateId kGoal = 1;
    constexpr AutomatonStateId kSink = 2;

    Automaton automaton;
    automaton.initialState = kInit;
    automaton.finalStates = {kGoal};
    automaton.transitions.assign(3, std::vector<AutomatonStateId>(totalLabels, kSink));

    for (std::size_t idx = 0; idx < totalLabels; ++idx) {
        automaton.transitions[kGoal][idx] = kGoal;
        automaton.transitions[kSink][idx] = kSink;
    }

    struct DfaKey {
        std::size_t stage;
        std::uint64_t forbidden;
        std::uint64_t visitedAvoid;
        bool operator==(const DfaKey& o) const noexcept {
            return stage == o.stage && forbidden == o.forbidden && visitedAvoid == o.visitedAvoid;
        }
    };

    struct DfaKeyHash {
        std::size_t operator()(const DfaKey& k) const noexcept {
            std::size_t h = std::hash<std::size_t>{}(k.stage);
            h ^= std::hash<std::uint64_t>{}(k.forbidden) + 0x9e3779b9ULL + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint64_t>{}(k.visitedAvoid) + 0x9e3779b9ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<DfaKey, AutomatonStateId, DfaKeyHash> keyToId;
    std::queue<std::pair<AutomatonStateId, DfaKey>> queue;

    const DfaKey initKey{0, 0ULL, 0ULL};
    keyToId.emplace(initKey, kInit);
    queue.push({kInit, initKey});

    while (!queue.empty()) {
        const auto [q, key] = queue.front();
        queue.pop();

        const auto& currentStage = stages[key.stage];

        for (std::size_t idx = 0; idx < totalLabels; ++idx) {
            const LabelId l = static_cast<LabelId>(idx);

            if (containsLabel(avoid, l)) {
                if (!useAvoidOr) {
                    automaton.transitions[q][idx] = kSink;
                } else {
                    const std::uint64_t nextVisited = key.visitedAvoid | (1ULL << l);
                    if (nextVisited == allAvoidMask) {
                        // Toutes les regions de Avoid ont ete touchees -> echec
                        automaton.transitions[q][idx] = kSink;
                    } else {
                        const DfaKey nextKey{key.stage, key.forbidden, nextVisited};
                        auto it = keyToId.find(nextKey);
                        if (it == keyToId.end()) {
                            const auto newId = static_cast<AutomatonStateId>(automaton.transitions.size());
                            automaton.transitions.emplace_back(totalLabels, kSink);
                            it = keyToId.emplace(nextKey, newId).first;
                            queue.push({newId, nextKey});
                        }
                        automaton.transitions[q][idx] = it->second;
                    }
                }
            } else if (l > 0 && ((key.forbidden >> l) & 1ULL) != 0ULL) {
                automaton.transitions[q][idx] = kSink;
            } else if (containsLabel(currentStage.labels, l)) {
                const std::size_t nextStage = key.stage + 1;
                if (nextStage == stages.size()) {
                    automaton.transitions[q][idx] = kGoal;
                } else {
                    std::uint64_t nextForbidden = key.forbidden;
                    if (currentStage.exclusive) {
                        for (LabelId other : currentStage.labels) {
                            if (other != l) {
                                nextForbidden |= (1ULL << other);
                            }
                        }
                    }
                    const DfaKey nextKey{nextStage, nextForbidden, key.visitedAvoid};
                    auto it = keyToId.find(nextKey);
                    if (it == keyToId.end()) {
                        const auto newId = static_cast<AutomatonStateId>(automaton.transitions.size());
                        automaton.transitions.emplace_back(totalLabels, kSink);
                        it = keyToId.emplace(nextKey, newId).first;
                        queue.push({newId, nextKey});
                    }
                    automaton.transitions[q][idx] = it->second;
                }
            } else {
                automaton.transitions[q][idx] = q;
            }
        }
    }

    validateAutomaton(automaton);
    return automaton;
}

symctrl::part2::StateLabels buildRegionLabels(const symctrl::IStateGrid& grid,
                                              const std::vector<RegionSpec>& regions,
                                              const MissionSpec& mission) {
    std::vector<bool> isAvoid(regions.size(), false);
    for (const std::string& raw : mission.avoid) {
        const std::string name = trim(raw);
        for (std::size_t i = 0; i < regions.size(); ++i) {
            if (trim(regions[i].name) == name) {
                isAvoid[i] = true;
            }
        }
    }

    symctrl::part2::StateLabels labels;
    const symctrl::StateId count = grid.internalStateCount();

    for (symctrl::StateId state = 1; state <= count; ++state) {
        const symctrl::Box cell = grid.cellBounds(state);
        symctrl::part2::LabelId assigned = 0;

        // 1. Priorite aux obstacles : sur-approximation (intersection)
        for (std::size_t i = 0; i < regions.size(); ++i) {
            if (isAvoid[i] && isCellIntersectingRegion2D(cell, regions[i])) {
                assigned = static_cast<symctrl::part2::LabelId>(i + 1);
                break;
            }
        }

        // 2. Cibles : sous-approximation (inclusion)
        if (assigned == 0) {
            for (std::size_t i = 0; i < regions.size(); ++i) {
                if (!isAvoid[i] && isCellInsideRegion2D(cell, regions[i])) {
                    assigned = static_cast<symctrl::part2::LabelId>(i + 1);
                    break;
                }
            }
        }

        labels[state] = assigned;
    }
    return labels;
}

} // namespace robot_app