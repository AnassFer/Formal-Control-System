#pragma once

#include "symctrl/part2/predecessor.hpp"

namespace symctrl::part2::detail {

bool keepsInside(const ITransitionSystem& model,
                 StateId state,
                 InputId input,
                 const StateSet& region);

} // namespace symctrl::part2::detail