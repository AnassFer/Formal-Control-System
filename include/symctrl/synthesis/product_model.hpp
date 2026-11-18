#pragma once

#include "symctrl/synthesis/finite_automaton.hpp"
#include "symctrl/synthesis/robust_predecessor.hpp"
#include "symctrl/transition_system.hpp"
#include <memory>

namespace symctrl::part2 {

struct ProductState {
    StateId systemState;
    AutomatonStateId automatonState;
};

// Input: systemStates: positive internal system-state count M; automatonStates: positive automaton-state
//        count Q.
// Output: Checked internal product count M * Q, with space for OUT also checked.
// Throws: std::invalid_argument for zero counts; std::overflow_error for count/container overflow.
StateId productStateCount(StateId systemStates, std::size_t automatonStates);

// System states start at 1 and automaton states start at 0.
// Input: state: system ID in 1..M; automatonState: q in 0..Q-1; systemStates/automatonStates: M/Q.
// Output: Product ID state + M*q in 1..M*Q; OUT is not encodable as a pair.
// Throws: Invalid-count/overflow errors or std::out_of_range for invalid component IDs.
StateId encodeProductState(StateId state, AutomatonStateId automatonState,
                           StateId systemStates, std::size_t automatonStates);

// State 0 is OUT and has no pair.
// Input: state: product ID in 1..M*Q; systemStates/automatonStates: positive M/Q.
// Output: ProductState containing the original internal system ID and zero-based automaton ID.
// Throws: Invalid-count/overflow errors or std::out_of_range for OUT/invalid product IDs.
ProductState decodeProductState(StateId state, StateId systemStates,
                                std::size_t automatonStates);

// OUT stays OUT. It is not a target of the product.
// Labels are needed for all internal system states.
// Input: model: system graph; automaton: task; labels: one label per internal system state; options: graph
//        budgets.
// Output: Shared immutable product graph; successor labels update task memory, and system OUT maps to product
//         OUT.
// Throws: Automaton/label/graph validation, storage-budget, checked-count or allocation errors.
std::shared_ptr<const ITransitionSystem> buildProductModel(
    const ITransitionSystem& model, const Automaton& automaton,
    const StateLabels& labels, BuildOptions options = {});

// Pairs with a final automaton state are in the target.
// Input: systemStates: positive internal system count M; automaton: validated task description.
// Output: All encoded pairs whose automaton component is final; may be empty; OUT is excluded.
// Throws: Automaton validation, count overflow or target-container length errors.
StateSet buildProductTarget(StateId systemStates, const Automaton& automaton);

// Read the first label before checking the winning domain.
// Input: systemStates: M; automaton: task; labels: complete state labels; winningDomain: internal winning
//        product IDs.
// Output: System IDs whose product pair is winning after consuming the initial physical-state label.
// Throws: Invalid counts, automaton/labels or winning product IDs; checked-count errors propagate.
StateSet computeWinningInitialStates(StateId systemStates, const Automaton& automaton,
                                     const StateLabels& labels, const StateSet& winningDomain);

} // namespace symctrl::part2
