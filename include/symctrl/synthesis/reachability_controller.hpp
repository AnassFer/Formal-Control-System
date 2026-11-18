#pragma once

#include "symctrl/synthesis/robust_predecessor.hpp"
#include <cstddef>
#include <map>
#include <vector>

namespace symctrl::part2 {

struct ReachabilityResult {
    StateSet domain;
    std::map<StateId, std::size_t> ranks;
};

// States in the target have rank 0.
// States that cannot reach the target have no rank.
// Input: model: transition graph; targetStates: valid internal target IDs.
// Output: ReachabilityResult with winning domain and iteration ranks; target rank is 0 and newly forced
//         predecessors get increasing ranks.
// Throws: std::out_of_range for invalid targets; rank overflow or graph errors propagate.
ReachabilityResult computeReachabilityDomain(const ITransitionSystem& model,
                                            const StateSet& targetStates);

using ReachabilityController = std::map<StateId, std::vector<InputId>>;

// Keep inputs that lead only to states with a lower rank.
// Target states have an empty input list because the goal is reached.
// Input: model: matching graph; result: winning domain and rank map from reachability synthesis.
// Output: Map of inputs whose successors all have smaller ranks; rank-0 states have empty lists because the
//         target is reached.
// Throws: Invalid IDs, inconsistent rank mappings, missing progress inputs or graph errors.
ReachabilityController buildReachabilityController(const ITransitionSystem& model,
                                                  const ReachabilityResult& result);

} // namespace symctrl::part2
