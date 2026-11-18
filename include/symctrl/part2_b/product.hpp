#pragma once

#include "symctrl/part2_b/automaton.hpp"
#include "symctrl/part2/predecessor.hpp"
#include "symctrl/contracts/transitions.hpp"
#include <memory>

namespace symctrl::part2 {

struct ProductState {
    StateId systemState;
    AutomatonStateId automatonState;
};

StateId productStateCount(StateId systemStates, std::size_t automatonStates);

// System states start at 1 and automaton states start at 0.
StateId encodeProductState(StateId state, AutomatonStateId automatonState,
                           StateId systemStates, std::size_t automatonStates);

// State 0 is OUT and has no pair.
ProductState decodeProductState(StateId state, StateId systemStates,
                                std::size_t automatonStates);

// OUT stays OUT. It is not a target of the product.
// Labels are needed for all internal system states.
std::shared_ptr<const ITransitionSystem> buildProductModel(
    const ITransitionSystem& model, const Automaton& automaton,
    const StateLabels& labels, BuildOptions options = {});

// Pairs with a final automaton state are in the target.
StateSet buildProductTarget(StateId systemStates, const Automaton& automaton);

// Read the first label before checking the winning domain.
StateSet computeWinningInitialStates(StateId systemStates, const Automaton& automaton,
                                     const StateLabels& labels, const StateSet& winningDomain);

} // namespace symctrl::part2
