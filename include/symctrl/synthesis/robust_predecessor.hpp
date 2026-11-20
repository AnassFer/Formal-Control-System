#pragma once

#include "symctrl/transition_system.hpp"

#include <set>

namespace symctrl::part2 {

using StateSet = std::set<StateId>;

// Returns the states with at least one input whose successor set
// is nonempty and entirely contained in region.
// Input: model: complete transition graph; region: desired successor-state set.
// Output: Internal states having at least one input with nonempty successors entirely inside region; OUT is
//         excluded.
// Throws: Graph-query or successor-representation errors propagate.
StateSet robustPredecessor(const ITransitionSystem& model,
                           const StateSet& region);

}
