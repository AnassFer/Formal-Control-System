#include "symctrl/synthesis/product_model.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace symctrl::part2 {
namespace {

// Input: labels: system-state-to-label map; states: internal count M; labelCount: number of automaton labels.
// Output: No return value; requires exactly one in-range label for every internal system state and leaves
//         labels unchanged.
// Throws: std::invalid_argument for missing coverage; std::out_of_range for invalid state/label IDs.
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
    // Input: states/inputs: counts; guarantee: numerical status; rows: prepared successor data; statistics:
    //        build totals.
    // Output: Constructs an immutable graph owning its rows and statistics; no return value. Caller supplies
    //         valid prepared data.
    ProductTransitionSystem(StateId states, InputId inputs, NumericGuarantee guarantee,
                            std::vector<SuccessorSet> rows, BuildStatistics statistics)
        : states_(states), inputs_(inputs), guarantee_(guarantee),
          rows_(std::move(rows)), statistics_(statistics) {}

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
        if (state > states_ || input >= inputs_) {
            throw std::out_of_range("product state or input out of range");
        }
        const auto index = checkedAdd(checkedMultiply(state, inputs_), input);
        return rows_.at(checkedSize(index));
    }
};

}

// Input: systemStates: positive internal system-state count M; automatonStates: positive automaton-state
//        count Q.
// Output: Checked internal product count M * Q, with space for OUT also checked.
// Throws: std::invalid_argument for zero counts; std::overflow_error for count/container overflow.
StateId productStateCount(StateId systemStates, std::size_t automatonStates) {
    if (systemStates == 0 || automatonStates == 0) {
        throw std::invalid_argument("system and automaton must have states");
    }

    const auto count = checkedMultiply(systemStates, automatonStates);
    checkedSize(checkedAdd(count, 1));
    return count;
}

// Input: state: system ID in 1..M; automatonState: q in 0..Q-1; systemStates/automatonStates: M/Q.
// Output: Product ID state + M*q in 1..M*Q; OUT is not encodable as a pair.
// Throws: Invalid-count/overflow errors or std::out_of_range for invalid component IDs.
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

// Input: state: product ID in 1..M*Q; systemStates/automatonStates: positive M/Q.
// Output: ProductState containing the original internal system ID and zero-based automaton ID.
// Throws: Invalid-count/overflow errors or std::out_of_range for OUT/invalid product IDs.
ProductState decodeProductState(StateId state, StateId systemStates,
                                std::size_t automatonStates) {
    const auto count = productStateCount(systemStates, automatonStates);

    if (state == kOutside || state > count) {
        throw std::out_of_range("product state out of range");
    }

    const auto index = state - 1;
    return ProductState{index % systemStates + 1, checkedSize(index / systemStates)};
}

// Input: model: system graph; automaton: task; labels: one label per internal system state; options: graph
//        budgets.
// Output: Shared immutable product graph; successor labels update task memory, and system OUT maps to product
//         OUT.
// Throws: Automaton/label/graph validation, storage-budget, checked-count or allocation errors.
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
            // Input: Callback receives next: a system successor ID; captures the current pair, automaton,
            //        labels and ids.
            // Output: No callback return value; appends OUT or the encoded successor/task-memory pair to ids.
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

// Input: systemStates: positive internal system count M; automaton: validated task description.
// Output: All encoded pairs whose automaton component is final; may be empty; OUT is excluded.
// Throws: Automaton validation, count overflow or target-container length errors.
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

// Input: systemStates: M; automaton: task; labels: complete state labels; winningDomain: internal winning
//        product IDs.
// Output: System IDs whose product pair is winning after consuming the initial physical-state label.
// Throws: Invalid counts, automaton/labels or winning product IDs; checked-count errors propagate.
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
