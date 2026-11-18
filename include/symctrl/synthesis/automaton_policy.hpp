#pragma once

#include "symctrl/policy.hpp"
#include "symctrl/synthesis/finite_automaton.hpp"
#include "symctrl/synthesis/reachability_controller.hpp"

namespace symctrl::part2 {

class AutomatonPolicy final : public ISymbolicPolicy {
    StateId systemStates_;
    InputId inputCount_;
    Automaton automaton_;
    StateLabels labels_;
    ReachabilityController controller_;

    // Input: previous_memory: task state before observation; current_state: internal system ID.
    // Output: Automaton state after consuming the label of current_state; the policy object is unchanged.
    // Throws: std::out_of_range for OUT, invalid system IDs or invalid memory.
    AutomatonStateId nextState(PolicyMemory previous_memory, StateId current_state) const;

public:
    // Use the controller computed on the product.
    // Input: systemStates/inputCount: system counts; automaton/labels: matching task data; controller:
    //        synthesized product actions.
    // Output: Constructs a policy owning the task, labels and controller; validates their structure; no
    //         return value.
    // Throws: Invalid counts, task, label coverage, product/action IDs or action-list structure.
    AutomatonPolicy(StateId systemStates, InputId inputCount, Automaton automaton,
                    StateLabels labels, ReachabilityController controller);

    // Input: None; reads this policy automaton.
    // Output: PolicyMemory holding the automaton initial state, before consuming the first observation.
    PolicyMemory initialMemory() const noexcept;

    // Check the goal before asking for an input.
    // Input: previous_memory: memory before this observation; current_state: internal system ID.
    // Output: True if consuming the current label reaches a final automaton state; memory is not mutated.
    // Throws: std::out_of_range for invalid state or memory.
    bool isFinished(PolicyMemory previous_memory, StateId current_state) const;

    // Read the current label before choosing an input.
    // Input: previous_memory: memory before this observation; current_state: internal system ID.
    // Output: PolicyDecision with updated automaton memory and the first allowed product-controller action.
    // Throws: std::out_of_range for invalid state/memory, an already reached goal or a missing controller
    //         entry.
    PolicyDecision decide(PolicyMemory previous_memory, StateId current_state) const override;
};

} // namespace symctrl::part2
