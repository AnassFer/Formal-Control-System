#pragma once

#include "symctrl/synthesis/robust_predecessor.hpp"

namespace symctrl::part2::detail {

bool keepsInside(const ITransitionSystem& model,
                 StateId state,
                 InputId input,
                 const StateSet& region);

} // namespace symctrl::part2::detail
