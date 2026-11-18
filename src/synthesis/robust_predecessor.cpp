#include "symctrl/synthesis/robust_predecessor.hpp"
#include "successor_condition.hpp"

namespace symctrl::part2 {
namespace detail {

// Input: model: graph; state/input: valid row IDs; region: permitted successor IDs.
// Output: True exactly when the successor row is nonempty and every successor belongs to region.
// Throws: Graph-query or successor-representation errors propagate.
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
    // Input: Callback receives successor: one successor ID; captures region and allInside by reference.
    // Output: No callback return value; sets allInside to false when a successor is outside region.
    forEachSuccessor(successors, stateCount, [&](StateId successor) {
        if (region.count(successor) == 0) {
            allInside = false;
        }
    });
    return allInside;
}

} // namespace

// Input: model: complete transition graph; region: desired successor-state set.
// Output: Internal states having at least one input with nonempty successors entirely inside region; OUT is
//         excluded.
// Throws: Graph-query or successor-representation errors propagate.
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
