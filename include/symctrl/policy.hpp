#pragma once
#include "symctrl/types.hpp"
namespace symctrl {
struct PolicyMemory { std::uint64_t value = 0; };
struct PolicyDecision { PolicyMemory next_memory; InputId input; };
struct ConcreteDecision { StateId state; InputId input; Vector physical_input; PolicyMemory next_memory; };
class ISymbolicPolicy {
public:
    // Input: None; called when this symbolic policy is destroyed.
    // Output: No return value; permits destruction of the concrete object through this interface.
    virtual ~ISymbolicPolicy() = default;
    // Caller passes psi(-1), default 0; update memory BEFORE choosing action.
    // Missing policy entry or invalid memory: std::out_of_range.
    // Input: previous_memory: memory before this observation; current_state: observed symbolic state.
    // Output: PolicyDecision containing updated memory and selected InputId; memory is updated logically
    //         before selecting the action.
    // Throws: std::out_of_range for invalid memory or a missing policy entry.
    virtual PolicyDecision decide(PolicyMemory previous_memory, StateId current_state) const = 0;
};
}
