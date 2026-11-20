#pragma once

#include "symctrl/synthesis/robust_predecessor.hpp"

namespace symctrl::part2::detail {

// Input: model: graph; state/input: valid row IDs; region: permitted successor IDs.
// Output: True exactly when the successor row is nonempty and every successor belongs to region.
// Throws: Graph-query or successor-representation errors propagate.
bool keepsInside(const ITransitionSystem& model,
                 StateId state,
                 InputId input,
                 const StateSet& region);

} // namespace symctrl::part2::detail
