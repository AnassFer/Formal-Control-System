#pragma once

#include "symctrl/part2/predecessor.hpp"
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
ReachabilityResult computeReachabilityDomain(const ITransitionSystem& model,
                                            const StateSet& targetStates);

using ReachabilityController = std::map<StateId, std::vector<InputId>>;

// Keep inputs that lead only to states with a lower rank.
// Target states have an empty input list because the goal is reached.
ReachabilityController buildReachabilityController(const ITransitionSystem& model,
                                                  const ReachabilityResult& result);

} // namespace symctrl::part2
