#pragma once
#include "symctrl/types.hpp"
#include <functional>

namespace symctrl {

    enum class SuccessorKind { Explicit, AllStates };

    struct SuccessorSet { SuccessorKind kind; std::vector<StateId> ids; };

    // Input: set: explicit or universal successors; internal_states: M, so valid IDs are 0..M.
    // Output: No return value; checks sorted unique explicit IDs or empty AllStates storage; set is
    //         unchanged.
    // Throws: std::invalid_argument for an unknown kind, invalid IDs/order or nonempty universal storage.
    void validateSuccessors(const SuccessorSet& set, StateId internal_states);

    // Input: set: successor representation; state: queried ID; internal_states: M.
    // Output: True when state is a successor; AllStates contains every ID in 0..M.
    // Throws: std::invalid_argument for an invalid set; std::out_of_range for an invalid queried ID.
    bool contains(const SuccessorSet& set, StateId state, StateId internal_states);

    // Input: set: successor representation; internal_states: M.
    // Output: False for empty Explicit; true for nonempty Explicit and for AllStates.
    // Throws: std::invalid_argument for an invalid successor representation.
    bool nonempty(const SuccessorSet& set, StateId internal_states);

    // Input: set: successors; internal_states: M; visit: callback taking one StateId.
    // Output: No return value; calls visit once per successor, expanding AllStates to 0..M; callback effects
    //         are retained.
    // Throws: std::invalid_argument for an invalid set or empty callback; callback exceptions propagate.
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

        // Input: None; called when this transition graph is destroyed.
        // Output: No return value; permits destruction of the concrete object through this interface.
        virtual ~ITransitionSystem() = default;

        // Input: None; reads this transition graph.
        // Output: Number M of internal states, excluding OUT.
        virtual StateId internalStateCount() const noexcept = 0;

        // Input: None; reads this transition graph.
        // Output: Number A of actions, indexed 0..A-1.
        virtual InputId inputCount() const noexcept = 0;

        // Input: None; reads this transition graph.
        // Output: Numerical-guarantee enum inherited from the source abstraction.
        virtual NumericGuarantee numericGuarantee() const noexcept = 0;

        // Input: None; reads this transition graph.
        // Output: Read-only construction statistics, valid for the graph lifetime.
        virtual const BuildStatistics& statistics() const noexcept = 0;

        // Input: state: ID in 0..M, including OUT; input: action ID in 0..A-1.
        // Output: Read-only successor row, explicit or AllStates, valid for the graph lifetime.
        // Throws: std::out_of_range for invalid state/action IDs.
        virtual const SuccessorSet& successors(StateId state, InputId input) const = 0;

    };

}
