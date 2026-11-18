#include "symctrl/part2/safety.hpp"
#include "symctrl/part2/successor_check.hpp"

namespace symctrl::part2 {

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