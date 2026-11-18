#include "symctrl/synthesis/finite_automaton.hpp"
#include <stdexcept>

namespace symctrl::part2 {

// Input: automaton: initial/final states and table indexed by [state][label].
// Output: No return value; checks a nonempty rectangular table and valid state references without modifying
//         the automaton.
// Throws: std::invalid_argument for empty/ragged tables; std::out_of_range for invalid state references.
void validateAutomaton(const Automaton& automaton) {
    const auto stateCount = automaton.transitions.size();
    if (stateCount == 0) {
        throw std::invalid_argument("automaton has no states");
    }

    const auto labelCount = automaton.transitions.front().size();
    if (labelCount == 0) {
        throw std::invalid_argument("automaton has no labels");
    }

    if (automaton.initialState >= stateCount) {
        throw std::out_of_range("initial automaton state out of range");
    }

    for (AutomatonStateId state : automaton.finalStates) {
        if (state >= stateCount) {
            throw std::out_of_range("final automaton state out of range");
        }
    }

    for (const auto& row : automaton.transitions) {
        if (row.size() != labelCount) {
            throw std::invalid_argument("automaton rows have different sizes");
        }
        for (AutomatonStateId state : row) {
            if (state >= stateCount) {
                throw std::out_of_range("next automaton state out of range");
            }
        }
    }
}

// Input: automaton: transition table; state: current zero-based automaton ID; label: zero-based label ID.
// Output: Destination automaton state automaton.transitions[state][label], after validation.
// Throws: Automaton validation errors or std::out_of_range for invalid state/label IDs.
AutomatonStateId nextAutomatonState(const Automaton& automaton,
                                   AutomatonStateId state,
                                   LabelId label) {
    validateAutomaton(automaton);

    if (state >= automaton.transitions.size()) {
        throw std::out_of_range("automaton state out of range");
    }
    if (label >= automaton.transitions[state].size()) {
        throw std::out_of_range("automaton label out of range");
    }

    return automaton.transitions[state][label];
}

}
