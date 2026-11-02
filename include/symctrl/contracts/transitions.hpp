#pragma once
#include "symctrl/contracts/types.hpp"
#include <functional>
namespace symctrl {
enum class SuccessorKind { Explicit, AllStates };
struct SuccessorSet { SuccessorKind kind; std::vector<StateId> ids; };
void validateSuccessors(const SuccessorSet& set, StateId internal_states);
bool contains(const SuccessorSet& set, StateId state, StateId internal_states);
bool nonempty(const SuccessorSet& set, StateId internal_states);
void forEachSuccessor(const SuccessorSet& set, StateId internal_states,
                      const std::function<void(StateId)>& visit);
struct BuildOptions {
    std::uint64_t max_state_input_pairs = 1000000; // Includes OUT/action pairs.
    std::uint64_t max_stored_edges = 10000000; // Explicit IDs only; universal rows stay compact.
    std::uint64_t max_estimated_bytes = 536870912;
};
struct BuildStatistics {
    std::uint64_t processed_pairs = 0;
    std::uint64_t stored_edges = 0;
    double abstraction_seconds = 0;
};
class ITransitionSystem {
public:
    virtual ~ITransitionSystem() = default;
    virtual StateId internalStateCount() const noexcept = 0;
    virtual InputId inputCount() const noexcept = 0;
    virtual NumericGuarantee numericGuarantee() const noexcept = 0;
    virtual const BuildStatistics& statistics() const noexcept = 0;
    virtual const SuccessorSet& successors(StateId state, InputId input) const = 0;
};
}
