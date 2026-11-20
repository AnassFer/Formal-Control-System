#pragma once
#include "symctrl/transition_system.hpp"
#include <memory>

namespace symctrl::detail {
// Persistent graph object and row headers; explicit ID buffers are budgeted separately.
// Input: pairs: total state/action row count, including OUT rows.
// Output: Estimated bytes for the graph object and row headers; explicit successor ID buffers are excluded.
// Throws: std::overflow_error if byte arithmetic overflows.
std::uint64_t explicitTransitionStorageBytes(std::uint64_t pairs);
// Input: states/inputs: counts; guarantee: numerical status; rows: prepared owned data; statistics: build
//        totals.
// Output: Shared immutable transition graph owning the supplied rows; caller supplies consistent prepared
//         data.
std::shared_ptr<const ITransitionSystem> makeExplicitTransitionSystem(
    StateId states, InputId inputs, NumericGuarantee guarantee,
    std::vector<SuccessorSet> rows, BuildStatistics statistics);
}
