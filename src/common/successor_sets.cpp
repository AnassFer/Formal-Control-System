#include "symctrl/transition_system.hpp"
#include <algorithm>

namespace symctrl {
// Input: set: explicit or universal successors; internal_states: M, so valid IDs are 0..M.
// Output: No return value; checks sorted unique explicit IDs or empty AllStates storage; set is unchanged.
// Throws: std::invalid_argument for an unknown kind, invalid IDs/order or nonempty universal storage.
void validateSuccessors(const SuccessorSet& set, StateId internal_states) {
    if (set.kind == SuccessorKind::AllStates) {
        if (!set.ids.empty()) throw std::invalid_argument("AllStates must have empty storage");
    } else if (set.kind == SuccessorKind::Explicit) {
        StateId previous = 0;
        bool first = true;
        for (StateId id : set.ids) {
            if (id > internal_states || (!first && id <= previous))
                throw std::invalid_argument("successors must be sorted, unique, in range");
            previous = id;
            first = false;
        }
    } else throw std::invalid_argument("unknown successor kind");
}
// Input: set: successor representation; state: queried ID; internal_states: M.
// Output: True when state is a successor; AllStates contains every ID in 0..M.
// Throws: std::invalid_argument for an invalid set; std::out_of_range for an invalid queried ID.
bool contains(const SuccessorSet& set, StateId state, StateId internal_states) {
    validateSuccessors(set, internal_states);
    if (state > internal_states) throw std::out_of_range("state ID");
    return set.kind == SuccessorKind::AllStates || std::binary_search(set.ids.begin(), set.ids.end(), state);
}
// Input: set: successor representation; internal_states: M.
// Output: False for empty Explicit; true for nonempty Explicit and for AllStates.
// Throws: std::invalid_argument for an invalid successor representation.
bool nonempty(const SuccessorSet& set, StateId internal_states) {
    validateSuccessors(set, internal_states);
    return set.kind == SuccessorKind::AllStates || !set.ids.empty();
}
// Input: set: successors; internal_states: M; visit: callback taking one StateId.
// Output: No return value; calls visit once per successor, expanding AllStates to 0..M; callback effects are
//         retained.
// Throws: std::invalid_argument for an invalid set or empty callback; callback exceptions propagate.
void forEachSuccessor(const SuccessorSet& set, StateId internal_states,
                      const std::function<void(StateId)>& visit) {
    validateSuccessors(set, internal_states);
    if (!visit) throw std::invalid_argument("empty successor visitor");
    if (set.kind == SuccessorKind::AllStates) {
        StateId id = 0;
        for (;;) { visit(id); if (id == internal_states) break; ++id; }
    } else for (StateId id : set.ids) visit(id);
}
}
