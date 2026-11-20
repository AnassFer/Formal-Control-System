#include "symctrl/synthesis/robust_predecessor.hpp"
#include "successor_condition.hpp"

#include <cstdint>
#include <vector>

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

    switch (successors.kind) {
        case SuccessorKind::AllStates:
            // AllStates includes OUT as well as every internal state.
            if (region.count(kOutside) == 0) {
                return false;
            }
            for (StateId index = 0; index < stateCount; ++index) {
                const StateId s = index + 1;
                if (region.count(s) == 0) {
                    return false;
                }
            }
            return true;

        case SuccessorKind::Explicit:
            for (StateId succ : successors.ids) {
                if (region.count(succ) == 0) {
                    return false;
                }
            }
            return !successors.ids.empty();
    }

    return false;
}

bool keepsInsideFast(const ITransitionSystem& model,
                     StateId state,
                     InputId input,
                     const std::vector<std::uint8_t>& inRegion) {
    const auto stateCount = model.internalStateCount();
    const auto& successors = model.successors(state, input);

    if (!nonempty(successors, stateCount)) {
        return false;
    }

    switch (successors.kind) {
        case SuccessorKind::AllStates:
            if (inRegion[0] == 0) {
                return false;
            }
            for (StateId index = 0; index < stateCount; ++index) {
                const StateId s = index + 1;
                if (inRegion[static_cast<std::size_t>(s)] == 0) {
                    return false;
                }
            }
            return true;

        case SuccessorKind::Explicit:
            if (successors.ids.empty()) {
                return false;
            }
            for (StateId succ : successors.ids) {
                if (inRegion[static_cast<std::size_t>(succ)] == 0) {
                    return false;
                }
            }
            return true;
    }

    return false;
}

} // namespace detail

// Input: model: complete transition graph; region: desired successor-state set.
// Output: Internal states having at least one input with nonempty successors entirely inside region; OUT is
//         excluded.
// Throws: Graph-query or successor-representation errors propagate.
StateSet robustPredecessor(const ITransitionSystem& model,
                           const StateSet& region) {
    StateSet result;
    if (region.empty()) {
        return result;
    }

    const auto stateCount = model.internalStateCount();
    const auto inputCount = model.inputCount();
    if (stateCount == 0 || inputCount == 0) {
        return result;
    }

    std::vector<std::uint8_t> inRegion(checkedSize(checkedAdd(stateCount, 1)), 0);
    for (StateId s : region) {
        if (s <= stateCount) {
            inRegion[static_cast<std::size_t>(s)] = 1;
        }
    }

    for (StateId index = 0; index < stateCount; ++index) {
        const StateId state = index + 1;
        for (InputId input = 0; input < inputCount; ++input) {
            if (detail::keepsInsideFast(model, state, input, inRegion)) {
                result.insert(state);
                break;
            }
        }
    }

    return result;
}

} // namespace symctrl::part2
