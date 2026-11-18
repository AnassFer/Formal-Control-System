#include "symctrl/synthesis/automaton_policy.hpp"
#include "symctrl/synthesis/product_model.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace symctrl::part2 {

// Input: systemStates/inputCount: system counts; automaton/labels: matching task data; controller:
//        synthesized product actions.
// Output: Constructs a policy owning the task, labels and controller; validates their structure; no return
//         value.
// Throws: Invalid counts, task, label coverage, product/action IDs or action-list structure.
AutomatonPolicy::AutomatonPolicy(StateId systemStates, InputId inputCount,
                                 Automaton automaton, StateLabels labels,
                                 ReachabilityController controller)
    : systemStates_(systemStates), inputCount_(inputCount), automaton_(std::move(automaton)),
      labels_(std::move(labels)), controller_(std::move(controller)) {
    validateAutomaton(automaton_);
    const auto automatonStates = automaton_.transitions.size();
    productStateCount(systemStates_, automatonStates);

    if (labels_.size() != checkedSize(systemStates_)) {
        throw std::invalid_argument("labels do not cover all system states");
    }
    for (const auto& entry : labels_) {
        if (entry.first == kOutside || entry.first > systemStates_) {
            throw std::out_of_range("labelled system state out of range");
        }
        if (entry.second >= automaton_.transitions.front().size()) {
            throw std::out_of_range("system label out of range");
        }
    }

    for (const auto& entry : controller_) {
        const auto pair = decodeProductState(entry.first, systemStates_, automatonStates);
        const bool final = automaton_.finalStates.count(pair.automatonState) != 0;
        const auto& inputs = entry.second;
        if (inputs.empty() != final) {
            throw std::invalid_argument("controller inputs do not match final states");
        }
        for (InputId input : inputs) {
            if (input >= inputCount_) {
                throw std::out_of_range("controller input out of range");
            }
        }
        if (!std::is_sorted(inputs.begin(), inputs.end())
            || std::adjacent_find(inputs.begin(), inputs.end()) != inputs.end()) {
            throw std::invalid_argument("controller inputs must be sorted and unique");
        }
    }
}

// Input: None; reads this policy automaton.
// Output: PolicyMemory holding the automaton initial state, before consuming the first observation.
PolicyMemory AutomatonPolicy::initialMemory() const noexcept {
    return PolicyMemory{static_cast<std::uint64_t>(automaton_.initialState)};
}

// Input: previous_memory: task state before observation; current_state: internal system ID.
// Output: Automaton state after consuming the label of current_state; the policy object is unchanged.
// Throws: std::out_of_range for OUT, invalid system IDs or invalid memory.
AutomatonStateId AutomatonPolicy::nextState(PolicyMemory previous_memory,
                                           StateId current_state) const {
    if (current_state == kOutside || current_state > systemStates_) {
        throw std::out_of_range("current system state out of range");
    }
    if (previous_memory.value >= automaton_.transitions.size()) {
        throw std::out_of_range("policy memory out of range");
    }

    const auto label = labels_.at(current_state);
    return automaton_.transitions[checkedSize(previous_memory.value)][label];
}

// Input: previous_memory: memory before this observation; current_state: internal system ID.
// Output: True if consuming the current label reaches a final automaton state; memory is not mutated.
// Throws: std::out_of_range for invalid state or memory.
bool AutomatonPolicy::isFinished(PolicyMemory previous_memory, StateId current_state) const {
    const auto q = nextState(previous_memory, current_state);
    return automaton_.finalStates.count(q) != 0;
}

// Input: previous_memory: memory before this observation; current_state: internal system ID.
// Output: PolicyDecision with updated automaton memory and the first allowed product-controller action.
// Throws: std::out_of_range for invalid state/memory, an already reached goal or a missing controller entry.
PolicyDecision AutomatonPolicy::decide(PolicyMemory previous_memory, StateId current_state) const {
    const auto q = nextState(previous_memory, current_state);
    if (automaton_.finalStates.count(q) != 0) {
        throw std::out_of_range("goal already reached");
    }

    const auto id = encodeProductState(current_state, q, systemStates_, automaton_.transitions.size());
    const auto entry = controller_.find(id);
    if (entry == controller_.end()) {
        throw std::out_of_range("no controller entry for the current state and memory");
    }

    return PolicyDecision{PolicyMemory{static_cast<std::uint64_t>(q)}, entry->second.front()};
}

}
