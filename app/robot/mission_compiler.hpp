#pragma once

#include "symctrl/state_grid.hpp"
#include "symctrl/synthesis/finite_automaton.hpp"
#include "symctrl/synthesis/product_model.hpp"

#include <array>
#include <string>
#include <vector>

namespace robot_app {

struct RegionSpec {
    std::string name;
    double x1Min = 0.0;
    double x1Max = 1.0;
    double x2Min = 0.0;
    double x2Max = 1.0;
    std::array<float, 4> color{0.2f, 0.6f, 0.9f, 0.35f};
};

struct MissionStage {
    std::vector<std::string> regions;
    bool exclusive = false;
};

struct MissionSpec {
    std::vector<MissionStage> stages;
    std::vector<std::string> avoid;
    bool avoidOr = false; // false = eviter TOUTES (ET), true = eviter AU MOINS UNE (OU)
};

symctrl::part2::Automaton compileMissionToAutomaton(const std::vector<RegionSpec>& regions,
                                                    const MissionSpec& mission);

symctrl::part2::StateLabels buildRegionLabels(const symctrl::IStateGrid& grid,
                                              const std::vector<RegionSpec>& regions,
                                              const MissionSpec& mission);

} // namespace robot_app