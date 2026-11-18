#include "symctrl/contracts/factories.hpp"
#include <stdexcept>

namespace symctrl {

// ---------------------------------------------------------------------------
// refineDecision — Part A policy adapter (§1.2)
//
// Sequence:
//   1. Locate the measured state in the grid.
//   2. Reject OUT (measured state outside the domain).
//   3. Ask the policy for an action, passing previous_memory.
//      The policy updates memory BEFORE choosing the action.
//   4. Validate the returned action ID against the catalogue.
//   5. Return the full ConcreteDecision with the stable physical vector ref
//      copied into an owned Vector.
//
// Errors:
//   std::out_of_range  — measured state maps to OUT, or action ID is invalid,
//                        or the policy itself throws out_of_range for a missing
//                        entry or invalid memory.
//   std::invalid_argument — validateVector inside locate() propagates for a
//                           mismatched or nonfinite measured_x.
// ---------------------------------------------------------------------------

ConcreteDecision refineDecision(const IStateGrid&    grid,
                                const IInputCatalog& inputs,
                                const ISymbolicPolicy& policy,
                                const Vector&        measured_x,
                                PolicyMemory         previous_memory) {
    // Step 1: map the physical measurement to a cell ID.
    const StateId state = grid.locate(measured_x); // throws invalid_argument on bad x

    // Step 2: an outside measurement cannot be refined.
    if (state == kOutside)
        throw std::out_of_range("refineDecision: measured state is outside the grid domain");

    // Step 3: ask the policy (may throw out_of_range for missing entry / bad memory).
    // The policy contract: update memory FIRST, then choose action using the new memory.
    const PolicyDecision decision = policy.decide(previous_memory, state);

    // Step 4: validate the returned action ID.
    if (decision.input >= inputs.inputCount())
        throw std::out_of_range("refineDecision: policy returned an invalid action ID");

    // Step 5: assemble the concrete decision.
    // inputs.value() returns a stable reference; copy into owned Vector.
    return ConcreteDecision{
        state,
        decision.input,
        inputs.value(decision.input), // Vector copy
        decision.next_memory
    };
}

} // namespace symctrl
