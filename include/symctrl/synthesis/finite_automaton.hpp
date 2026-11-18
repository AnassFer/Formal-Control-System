#pragma once

#include "symctrl/types.hpp"
#include <cstddef>
#include <map>
#include <set>
#include <vector>

namespace symctrl::part2 {

using AutomatonStateId = std::size_t;
using LabelId = std::size_t;
using StateLabels = std::map<StateId, LabelId>;

struct Automaton {
    AutomatonStateId initialState = 0;
    std::set<AutomatonStateId> finalStates;
    std::vector<std::vector<AutomatonStateId>> transitions;
};

// Each row is a state and each column is a label.
// Input: automaton: initial/final states and table indexed by [state][label].
// Output: No return value; checks a nonempty rectangular table and valid state references without modifying
//         the automaton.
// Throws: std::invalid_argument for empty/ragged tables; std::out_of_range for invalid state references.
void validateAutomaton(const Automaton& automaton);

// Input: automaton: transition table; state: current zero-based automaton ID; label: zero-based label ID.
// Output: Destination automaton state automaton.transitions[state][label], after validation.
// Throws: Automaton validation errors or std::out_of_range for invalid state/label IDs.
AutomatonStateId nextAutomatonState(const Automaton& automaton,
                                   AutomatonStateId state,
                                   LabelId label);

} // namespace symctrl::part2
