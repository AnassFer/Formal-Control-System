#include "transition_storage.hpp"
#include <utility>

namespace symctrl {
namespace {
class ExplicitTransitionSystem final : public ITransitionSystem {
    const StateId states_;
    const InputId inputs_;
    const NumericGuarantee guarantee_;
    const std::vector<SuccessorSet> rows_;
    const BuildStatistics statistics_;

public:
    // Input: states/inputs: counts; guarantee: numerical status; rows: prepared successor data; statistics:
    //        build totals.
    // Output: Constructs an immutable graph owning its rows and statistics; no return value. Caller supplies
    //         valid prepared data.
    ExplicitTransitionSystem(StateId states, InputId inputs, NumericGuarantee guarantee,
                             std::vector<SuccessorSet> rows, BuildStatistics statistics)
        : states_(states), inputs_(inputs), guarantee_(guarantee), rows_(std::move(rows)),
          statistics_(statistics) {}

    // Input: None; reads this transition graph.
    // Output: Number M of internal states, excluding OUT.
    StateId internalStateCount() const noexcept override { return states_; }
    // Input: None; reads this transition graph.
    // Output: Number A of actions, indexed 0..A-1.
    InputId inputCount() const noexcept override { return inputs_; }
    // Input: None; reads this transition graph.
    // Output: Numerical-guarantee enum inherited from the source abstraction.
    NumericGuarantee numericGuarantee() const noexcept override { return guarantee_; }
    // Input: None; reads this transition graph.
    // Output: Read-only construction statistics, valid for the graph lifetime.
    const BuildStatistics& statistics() const noexcept override { return statistics_; }
    // Input: state: ID in 0..M, including OUT; input: action ID in 0..A-1.
    // Output: Read-only successor row, explicit or AllStates, valid for the graph lifetime.
    // Throws: std::out_of_range for invalid state/action IDs.
    const SuccessorSet& successors(StateId state, InputId input) const override {
        if (state > states_ || input >= inputs_) throw std::out_of_range("transition state/input ID");
        return rows_.at(checkedSize(checkedAdd(checkedMultiply(state, inputs_), input)));
    }
};
}

namespace detail {
// Input: pairs: total state/action row count, including OUT rows.
// Output: Estimated bytes for the graph object and row headers; explicit successor ID buffers are excluded.
// Throws: std::overflow_error if byte arithmetic overflows.
std::uint64_t explicitTransitionStorageBytes(std::uint64_t pairs) {
    return checkedAdd(sizeof(ExplicitTransitionSystem), checkedMultiply(pairs, sizeof(SuccessorSet)));
}

// Input: states/inputs: counts; guarantee: numerical status; rows: prepared owned data; statistics: build
//        totals.
// Output: Shared immutable transition graph owning the supplied rows; caller supplies consistent prepared
//         data.
std::shared_ptr<const ITransitionSystem> makeExplicitTransitionSystem(
    StateId states, InputId inputs, NumericGuarantee guarantee,
    std::vector<SuccessorSet> rows, BuildStatistics statistics) {
    return std::make_shared<ExplicitTransitionSystem>(
        states, inputs, guarantee, std::move(rows), statistics);
}
}
}
