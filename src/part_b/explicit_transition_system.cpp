#include "symctrl/part_b/transition_storage.hpp"
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
    ExplicitTransitionSystem(StateId states, InputId inputs, NumericGuarantee guarantee,
                             std::vector<SuccessorSet> rows, BuildStatistics statistics)
        : states_(states), inputs_(inputs), guarantee_(guarantee), rows_(std::move(rows)),
          statistics_(statistics) {}

    StateId internalStateCount() const noexcept override { return states_; }
    InputId inputCount() const noexcept override { return inputs_; }
    NumericGuarantee numericGuarantee() const noexcept override { return guarantee_; }
    const BuildStatistics& statistics() const noexcept override { return statistics_; }
    const SuccessorSet& successors(StateId state, InputId input) const override {
        if (state > states_ || input >= inputs_) throw std::out_of_range("transition state/input ID");
        return rows_.at(checkedSize(checkedAdd(checkedMultiply(state, inputs_), input)));
    }
};
}

namespace detail {
std::uint64_t explicitTransitionStorageBytes(std::uint64_t pairs) {
    return checkedAdd(sizeof(ExplicitTransitionSystem), checkedMultiply(pairs, sizeof(SuccessorSet)));
}

std::shared_ptr<const ITransitionSystem> makeExplicitTransitionSystem(
    StateId states, InputId inputs, NumericGuarantee guarantee,
    std::vector<SuccessorSet> rows, BuildStatistics statistics) {
    return std::make_shared<ExplicitTransitionSystem>(
        states, inputs, guarantee, std::move(rows), statistics);
}
}
}
