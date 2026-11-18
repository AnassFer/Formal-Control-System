#pragma once

#include "symctrl/contracts/policy.hpp"
#include "symctrl/part2_b/automaton.hpp"
#include "symctrl/part2_b/reachability.hpp"

namespace symctrl::part2 {

class AutomatonPolicy final : public ISymbolicPolicy {
    StateId systemStates_;
    InputId inputCount_;
    Automaton automaton_;
    StateLabels labels_;
    ReachabilityController controller_;

    AutomatonStateId nextState(PolicyMemory previous_memory, StateId current_state) const;

public:
    // Use the controller computed on the product.
    AutomatonPolicy(StateId systemStates, InputId inputCount, Automaton automaton,
                    StateLabels labels, ReachabilityController controller);

    PolicyMemory initialMemory() const noexcept;

    // Check the goal before asking for an input.
    bool isFinished(PolicyMemory previous_memory, StateId current_state) const;

    // Read the current label before choosing an input.
    PolicyDecision decide(PolicyMemory previous_memory, StateId current_state) const override;
};

} // namespace symctrl::part2
