#pragma once

#include "symctrl/synthesis/robust_predecessor.hpp"
#include <map>
#include <vector>

namespace symctrl::part2 {

// Input: model: transition graph; safeStates: candidate safe set of valid internal state IDs.
// Output: Greatest controlled invariant subset of safeStates, found by repeated intersection with robust
//         predecessors.
// Throws: Graph-query or successor-representation errors propagate; caller supplies valid safe-state IDs.
StateSet computeSafetyDomain(const ITransitionSystem& model,
                             const StateSet& safeStates);

using SafetyController = std::map<StateId, std::vector<InputId>>;

// Builds a safety controller that allows only inputs that keep the system
// inside the safety domain.
// Pass the fixed point returned by computeSafetyDomain.
// Input: model: transition graph; safetyDomain: invariant domain, normally returned by computeSafetyDomain.
// Output: Map from each domain state to all inputs keeping every successor in the domain; an arbitrary domain
//         may yield empty lists.
// Throws: Graph-query or successor-representation errors propagate.
SafetyController buildSafetyController(const ITransitionSystem& model,
                                       const StateSet& safetyDomain);

} // namespace symctrl::part2
