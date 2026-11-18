#pragma once

#include "symctrl/synthesis/robust_predecessor.hpp"
#include <map>
#include <vector>

namespace symctrl::part2 {

StateSet computeSafetyDomain(const ITransitionSystem& model,
                             const StateSet& safeStates);

using SafetyController = std::map<StateId, std::vector<InputId>>;

// Builds a safety controller that allows only inputs that keep the system
// inside the safety domain.
// Pass the fixed point returned by computeSafetyDomain.
SafetyController buildSafetyController(const ITransitionSystem& model,
                                       const StateSet& safetyDomain);

} // namespace symctrl::part2
