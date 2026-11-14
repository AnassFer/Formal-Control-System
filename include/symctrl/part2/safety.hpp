#pragma once

#include "symctrl/part2/predecessor.hpp"
#include <map>
#include <vector>

namespace symctrl::part2 {

StateSet computeSafetyDomain(const ITransitionSystem& model,
                             const StateSet& safeStates);

using SafetyController = std::map<StateId, std::vector<InputId>>;

// Builds a safety controller that allows only inputs that keep the system
// inside the safety domain.
// The safety domain is passed in using robustPredecessor defined in predecessor.hpp.
SafetyController buildSafetyController(const ITransitionSystem& model,
                                       const StateSet& safetyDomain);

} // namespace symctrl::part2