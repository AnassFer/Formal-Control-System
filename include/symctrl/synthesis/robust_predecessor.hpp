#pragma once

#include "symctrl/transition_system.hpp"

#include <set>

namespace symctrl::part2 {

using StateSet = std::set<StateId>;

// Returns the states with at least one input whose successor set
// is nonempty and entirely contained in region.
StateSet robustPredecessor(const ITransitionSystem& model,
                           const StateSet& region);

}
