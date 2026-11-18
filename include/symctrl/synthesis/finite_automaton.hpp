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
void validateAutomaton(const Automaton& automaton);

AutomatonStateId nextAutomatonState(const Automaton& automaton,
                                   AutomatonStateId state,
                                   LabelId label);

} // namespace symctrl::part2
