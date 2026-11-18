#pragma once
#include "symctrl/transition_system.hpp"
#include <memory>

namespace symctrl::detail {
// Persistent graph object and row headers; explicit ID buffers are budgeted separately.
std::uint64_t explicitTransitionStorageBytes(std::uint64_t pairs);
std::shared_ptr<const ITransitionSystem> makeExplicitTransitionSystem(
    StateId states, InputId inputs, NumericGuarantee guarantee,
    std::vector<SuccessorSet> rows, BuildStatistics statistics);
}
