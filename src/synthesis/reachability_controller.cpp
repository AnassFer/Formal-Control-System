#include "symctrl/synthesis/reachability_controller.hpp"
#include "successor_condition.hpp"
#include <limits>
#include <stdexcept>

namespace symctrl::part2 {

ReachabilityResult computeReachabilityDomain(const ITransitionSystem& model,
                                            const StateSet& targetStates) {
    ReachabilityResult result;

    for (StateId state : targetStates) {
        if (state == kOutside || state > model.internalStateCount()) {
            throw std::out_of_range("target state ID out of range");
        }

        result.domain.insert(state);
        result.ranks[state] = 0;
    }

    std::size_t rank = 0;

    while (true) {
        const StateSet predecessor = robustPredecessor(model, result.domain);
        StateSet next = result.domain;
        next.insert(predecessor.begin(), predecessor.end());

        if (next == result.domain) {
            return result;
        }

        if (rank == std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error("reachability rank overflow");
        }
        ++rank;

        for (StateId state : next) {
            if (result.domain.count(state) == 0) {
                result.ranks[state] = rank;
            }
        }

        result.domain = next;
    }
}

ReachabilityController buildReachabilityController(const ITransitionSystem& model,
                                                  const ReachabilityResult& result) {
    if (result.domain.size() != result.ranks.size()) {
        throw std::invalid_argument("domain and ranks do not match");
    }

    for (StateId state : result.domain) {
        if (state == kOutside || state > model.internalStateCount()) {
            throw std::out_of_range("winning state ID out of range");
        }
        if (result.ranks.count(state) == 0) {
            throw std::invalid_argument("winning state has no rank");
        }
    }

    ReachabilityController controller;
    const auto inputCount = model.inputCount();

    for (StateId state : result.domain) {
        auto& allowedInputs = controller[state];
        const auto rank = result.ranks.at(state);

        if (rank == 0) {
            continue;
        }

        StateSet lower;
        for (StateId next : result.domain) {
            if (result.ranks.at(next) < rank) {
                lower.insert(next);
            }
        }

        for (InputId input = 0; input < inputCount; ++input) {
            if (detail::keepsInside(model, state, input, lower)) {
                allowedInputs.push_back(input);
            }
        }

        if (allowedInputs.empty()) {
            throw std::invalid_argument("winning state has no input with lower ranks");
        }
    }

    return controller;
}

}
