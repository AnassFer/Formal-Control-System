#pragma once
#include "symctrl/contracts/types.hpp"
namespace symctrl {
struct PolicyMemory { std::uint64_t value = 0; };
struct PolicyDecision { PolicyMemory next_memory; InputId input; };
struct ConcreteDecision { StateId state; InputId input; Vector physical_input; PolicyMemory next_memory; };
class ISymbolicPolicy {
public:
    virtual ~ISymbolicPolicy() = default;
    // Caller passes psi(-1), default 0; update memory BEFORE choosing action.
    // Missing policy entry or invalid memory: std::out_of_range.
    virtual PolicyDecision decide(PolicyMemory previous_memory, StateId current_state) const = 0;
};
}
