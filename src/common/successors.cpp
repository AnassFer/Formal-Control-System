#include "symctrl/contracts/transitions.hpp"
#include <algorithm>
namespace symctrl {
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
bool contains(const SuccessorSet& set, StateId state, StateId internal_states) {
    validateSuccessors(set, internal_states);
    if (state > internal_states) throw std::out_of_range("state ID");
    return set.kind == SuccessorKind::AllStates || std::binary_search(set.ids.begin(), set.ids.end(), state);
}
bool nonempty(const SuccessorSet& set, StateId internal_states) {
    validateSuccessors(set, internal_states);
    return set.kind == SuccessorKind::AllStates || !set.ids.empty();
}
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
