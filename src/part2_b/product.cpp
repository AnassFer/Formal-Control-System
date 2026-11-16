#include "symctrl/part2_b/product.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace symctrl::part2 {
namespace {

void validateLabels(const StateLabels& labels, StateId states, std::size_t labelCount) {
    if (labels.size() != checkedSize(states)) {
        throw std::invalid_argument("labels do not cover all system states");
    }
    for (const auto& entry : labels) {
        if (entry.first == kOutside || entry.first > states) {
            throw std::out_of_range("labelled system state out of range");
        }
        if (entry.second >= labelCount) {
            throw std::out_of_range("system label out of range");
        }
    }
}

class ProductTransitionSystem final : public ITransitionSystem {
    const StateId states_;
    const InputId inputs_;
    const NumericGuarantee guarantee_;
    const std::vector<SuccessorSet> rows_;
    const BuildStatistics statistics_;

public:
    ProductTransitionSystem(StateId states, InputId inputs, NumericGuarantee guarantee,
                            std::vector<SuccessorSet> rows, BuildStatistics statistics)
        : states_(states), inputs_(inputs), guarantee_(guarantee),
          rows_(std::move(rows)), statistics_(statistics) {}

    StateId internalStateCount() const noexcept override { return states_; }
    InputId inputCount() const noexcept override { return inputs_; }
    NumericGuarantee numericGuarantee() const noexcept override { return guarantee_; }
    const BuildStatistics& statistics() const noexcept override { return statistics_; }

    const SuccessorSet& successors(StateId state, InputId input) const override {
        if (state > states_ || input >= inputs_) {
            throw std::out_of_range("product state or input out of range");
        }
        const auto index = checkedAdd(checkedMultiply(state, inputs_), input);
        return rows_.at(checkedSize(index));
    }
};

}

StateId productStateCount(StateId systemStates, std::size_t automatonStates) {
    if (systemStates == 0 || automatonStates == 0) {
        throw std::invalid_argument("system and automaton must have states");
    }

    const auto count = checkedMultiply(systemStates, automatonStates);
    checkedSize(checkedAdd(count, 1));
    return count;
}

StateId encodeProductState(StateId state, AutomatonStateId automatonState,
                           StateId systemStates, std::size_t automatonStates) {
    productStateCount(systemStates, automatonStates);

    if (state == kOutside || state > systemStates) {
        throw std::out_of_range("system state out of range");
    }
    if (automatonState >= automatonStates) {
        throw std::out_of_range("automaton state out of range");
    }

    return checkedAdd(state, checkedMultiply(systemStates, automatonState));
}

ProductState decodeProductState(StateId state, StateId systemStates,
                                std::size_t automatonStates) {
    const auto count = productStateCount(systemStates, automatonStates);

    if (state == kOutside || state > count) {
        throw std::out_of_range("product state out of range");
    }

    const auto index = state - 1;
    return ProductState{index % systemStates + 1, checkedSize(index / systemStates)};
}

std::shared_ptr<const ITransitionSystem> buildProductModel(
    const ITransitionSystem& model, const Automaton& automaton,
    const StateLabels& labels, BuildOptions options) {
    const auto started = std::chrono::steady_clock::now();
    validateAutomaton(automaton);
    const auto states = model.internalStateCount();
    const auto actions = model.inputCount();
    const auto automatonStates = automaton.transitions.size();
    const auto count = productStateCount(states, automatonStates);
    const auto labelCount = automaton.transitions.front().size();
    const auto guarantee = model.numericGuarantee();
    (void)toString(guarantee);

    validateLabels(labels, states, labelCount);

    const auto pairs = checkedMultiply(checkedAdd(count, 1), actions);
    if (pairs > options.max_state_input_pairs) {
        throw std::length_error("product state/input pair budget exceeded");
    }
    const auto rowCount = checkedSize(pairs);
    if (rowCount > std::vector<SuccessorSet>().max_size()) {
        throw std::length_error("product row container limit exceeded");
    }
    auto bytes = checkedAdd(sizeof(ProductTransitionSystem),
                            checkedMultiply(pairs, sizeof(SuccessorSet)));
    if (bytes > options.max_estimated_bytes) {
        throw std::length_error("product byte budget exceeded");
    }

    std::vector<SuccessorSet> rows(rowCount, {SuccessorKind::Explicit, {}});
    bytes = checkedAdd(sizeof(ProductTransitionSystem),
                       checkedMultiply(rows.capacity(), sizeof(SuccessorSet)));
    if (bytes > options.max_estimated_bytes) {
        throw std::length_error("product byte budget exceeded");
    }
    for (InputId input = 0; input < actions; ++input) {
        const auto& outside = model.successors(kOutside, input);
        validateSuccessors(outside, states);
        if (outside.kind != SuccessorKind::AllStates) {
            throw std::invalid_argument("system OUT successors must be AllStates");
        }
        rows[checkedSize(input)].kind = SuccessorKind::AllStates;
    }

    BuildStatistics statistics;
    for (StateId state = 1; state <= count; ++state) {
        const auto pair = decodeProductState(state, states, automatonStates);
        for (InputId input = 0; input < actions; ++input) {
            const auto& successors = model.successors(pair.systemState, input);
            validateSuccessors(successors, states);
            const auto edges = successors.kind == SuccessorKind::AllStates
                ? checkedAdd(states, 1) : successors.ids.size();
            const auto totalEdges = checkedAdd(statistics.stored_edges, edges);
            if (totalEdges > options.max_stored_edges) {
                throw std::length_error("product successor budget exceeded");
            }
            if (checkedAdd(bytes, checkedMultiply(edges, sizeof(StateId))) > options.max_estimated_bytes) {
                throw std::length_error("product byte budget exceeded");
            }
            std::vector<StateId> ids;
            const auto size = checkedSize(edges);
            if (size > ids.max_size()) {
                throw std::length_error("product successor container limit exceeded");
            }
            ids.reserve(size);
            bytes = checkedAdd(bytes, checkedMultiply(ids.capacity(), sizeof(StateId)));
            if (bytes > options.max_estimated_bytes) {
                throw std::length_error("product byte budget exceeded");
            }
            forEachSuccessor(successors, states, [&](StateId next) {
                if (next == kOutside) {
                    ids.push_back(kOutside);
                } else {
                    const auto label = labels.at(next);
                    const auto q = automaton.transitions[pair.automatonState][label];
                    ids.push_back(encodeProductState(next, q, states, automatonStates));
                }
            });
            std::sort(ids.begin(), ids.end());
            const auto index = checkedAdd(checkedMultiply(state, actions), input);
            rows[checkedSize(index)].ids = std::move(ids);
            statistics.stored_edges = totalEdges;
        }
    }
    statistics.processed_pairs = pairs;
    statistics.abstraction_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    return std::make_shared<ProductTransitionSystem>(count, actions, guarantee,
                                                    std::move(rows), statistics);
}

StateSet buildProductTarget(StateId systemStates, const Automaton& automaton) {
    validateAutomaton(automaton);
    const auto automatonStates = automaton.transitions.size();
    productStateCount(systemStates, automatonStates);

    StateSet target;
    const auto count = checkedMultiply(systemStates, automaton.finalStates.size());
    if (count > target.max_size()) {
        throw std::length_error("product target container limit exceeded");
    }

    for (AutomatonStateId q : automaton.finalStates) {
        for (StateId state = 1; state <= systemStates; ++state) {
            target.insert(encodeProductState(state, q, systemStates, automatonStates));
        }
    }

    return target;
}

StateSet computeWinningInitialStates(StateId systemStates, const Automaton& automaton,
                                     const StateLabels& labels, const StateSet& winningDomain) {
    validateAutomaton(automaton);
    const auto automatonStates = automaton.transitions.size();
    const auto count = productStateCount(systemStates, automatonStates);
    validateLabels(labels, systemStates, automaton.transitions.front().size());

    for (StateId state : winningDomain) {
        if (state == kOutside || state > count) {
            throw std::out_of_range("winning product state out of range");
        }
    }

    StateSet result;
    for (StateId state = 1; state <= systemStates; ++state) {
        const auto label = labels.at(state);
        const auto q = automaton.transitions[automaton.initialState][label];
        const auto id = encodeProductState(state, q, systemStates, automatonStates);
        if (winningDomain.count(id) != 0) {
            result.insert(state);
        }
    }

    return result;
}

}
