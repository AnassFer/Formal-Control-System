#include "symctrl/synthesis/safety_controller.hpp"
#include "successor_condition.hpp"

namespace symctrl::part2 {

// Input: model: transition graph; safeStates: candidate safe set of valid internal state IDs.
// Output: Greatest controlled invariant subset of safeStates, found by repeated intersection with robust
//         predecessors.
// Throws: Graph-query or successor-representation errors propagate; caller supplies valid safe-state IDs.
StateSet computeSafetyDomain(const ITransitionSystem& model,
                             const StateSet& safeStates) {
    StateSet domain = safeStates;

    while (true) {
        const StateSet predecessor = robustPredecessor(model, domain);
        StateSet nextDomain;

        for (StateId state : domain) {
            if (predecessor.count(state) != 0) {
                nextDomain.insert(state);
            }
        }

        if (nextDomain == domain) {
            return domain;
        }

        domain = nextDomain;
    }
}

// Input: model: transition graph; safetyDomain: invariant domain, normally returned by computeSafetyDomain.
// Output: Map from each domain state to all inputs keeping every successor in the domain; an arbitrary domain
//         may yield empty lists.
// Throws: Graph-query or successor-representation errors propagate.
SafetyController buildSafetyController(const ITransitionSystem& model,
                                       const StateSet& safetyDomain) {
    SafetyController controller;
    const auto inputCount = model.inputCount();

    for (StateId state : safetyDomain) {
        auto& allowedInputs = controller[state];

        for (InputId input = 0; input < inputCount; ++input) {
            if (detail::keepsInside(model, state, input, safetyDomain)) {
                allowedInputs.push_back(input);
            }
        }
    }

    return controller;
}

} // namespace symctrl::part2
