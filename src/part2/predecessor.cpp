#include "symctrl/part2/predecessor.hpp"
#include "symctrl/part2/successor_check.hpp"

namespace symctrl::part2 {
namespace detail {

bool keepsInside(const ITransitionSystem& model,
                 StateId state,
                 InputId input,
                 const StateSet& region) {
    const auto stateCount = model.internalStateCount();
    const auto& successors = model.successors(state, input);

    if (!nonempty(successors, stateCount)) {
        return false;
    }

    bool allInside = true;
    forEachSuccessor(successors, stateCount, [&](StateId successor) {
        if (region.count(successor) == 0) {
            allInside = false;
        }
    });
    return allInside;
}

} // namespace

StateSet robustPredecessor(const ITransitionSystem& model,
                           const StateSet& region) {
    StateSet result;
    const auto stateCount = model.internalStateCount();
    const auto inputCount = model.inputCount();

    for (StateId index = 0; index < stateCount; ++index) {
        const StateId state = index + 1;
        for (InputId input = 0; input < inputCount; ++input) {
            if (detail::keepsInside(model, state, input, region)) {
                result.insert(state);
                break;
            }
        }
    }

    return result;
}

} // namespace symctrl::part2