#include "symctrl/factories.hpp"
#include "transition_storage.hpp"
#include <chrono>
#include <utility>

namespace symctrl {
// Input: grid, inputs, reachability: compatible views; disturbance: q-dimensional box; options: graph
//        budgets.
// Output: Shared immutable graph owning all state/action successor rows, including universal OUT rows and
//         build statistics.
// Throws: std::invalid_argument for inconsistent data; std::length_error for exceeded budgets;
//         arithmetic/backend errors propagate.
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid& grid,
    const IInputCatalog& inputs, const IReachability& reachability,
    const Box& disturbance, BuildOptions options) {
    const auto started = std::chrono::steady_clock::now();
    const auto& spec = grid.spec();
    const StateId states = validateGridSpec(spec);
    const InputId actions = inputs.inputCount();
    const auto n = grid.stateDimension(), m = inputs.inputDimension();
    if (n != spec.domain.lo.size() || n != reachability.stateDimension()
        || m == 0 || m != reachability.inputDimension() || actions == 0
        || states != grid.internalStateCount())
        throw std::invalid_argument("grid/input/reachability dimensions or counts disagree");
    validateBox(disturbance, true);
    validateVector(disturbance.lo, reachability.disturbanceDimension());
    const auto guarantee = reachability.numericGuarantee();
    (void)toString(guarantee);

    const auto pairs = checkedMultiply(checkedAdd(states, 1), actions);
    if (pairs > options.max_state_input_pairs)
        throw std::length_error("state/input pair budget exceeded");
    const auto rowCount = checkedSize(pairs);
    if (rowCount > std::vector<SuccessorSet>().max_size())
        throw std::length_error("transition row container limit exceeded");
    auto estimatedBytes = detail::explicitTransitionStorageBytes(pairs);
    if (estimatedBytes > options.max_estimated_bytes)
        throw std::length_error("transition byte budget exceeded");

    std::vector<SuccessorSet> rows(rowCount, SuccessorSet{SuccessorKind::Explicit, {}});
    // Account for the vector's actual reserved row storage on this standard library.
    estimatedBytes = detail::explicitTransitionStorageBytes(rows.capacity());
    if (estimatedBytes > options.max_estimated_bytes)
        throw std::length_error("transition byte budget exceeded");
    for (InputId action = 0; action < actions; ++action)
        rows[checkedSize(action)].kind = SuccessorKind::AllStates;

    BuildStatistics statistics;
    Box cell;
    for (StateId state = 1; state <= states; ++state) {
        cell = grid.cellBounds(state);
        validateBox(cell);
        validateVector(cell.lo, n);
        for (std::size_t d = 0; d < n; ++d)
            if (cell.lo[d] < spec.domain.lo[d] || cell.hi[d] > spec.domain.hi[d]
                || !(cell.lo[d] < cell.hi[d]))
                throw std::invalid_argument("invalid internal cell geometry");

        for (InputId action = 0; action < actions; ++action) {
            const auto& input = inputs.value(action);
            validateVector(input, m);
            const auto enclosure = reachability.enclose(cell, input, disturbance);
            validateBox(enclosure);
            validateVector(enclosure.lo, n);
            bool outside = false;
            for (std::size_t d = 0; d < n; ++d) {
                if (!spec.periodic.empty() && spec.periodic[d]) continue;
                outside = outside || enclosure.lo[d] <= spec.domain.lo[d]
                                  || enclosure.hi[d] >= spec.domain.hi[d];
            }

            const auto hits = grid.intersectingInternalCells(enclosure);
            StateId previous = 0;
            for (StateId hit : hits) {
                if (hit == 0 || hit > states || hit <= previous)
                    throw std::invalid_argument("internal intersections must be sorted unique internal IDs");
                previous = hit;
            }
            const auto rowEdges = checkedAdd(hits.size(), outside ? 1 : 0);
            if (rowEdges == 0)
                throw std::invalid_argument("empty successor row for a complete model");
            const auto totalEdges = checkedAdd(statistics.stored_edges, rowEdges);
            if (totalEdges > options.max_stored_edges)
                throw std::length_error("stored successor budget exceeded");
            const auto count = checkedSize(rowEdges);
            if (count > std::vector<StateId>().max_size())
                throw std::length_error("successor container limit exceeded");
            const auto requestedBytes = checkedMultiply(rowEdges, sizeof(StateId));
            if (checkedAdd(estimatedBytes, requestedBytes) > options.max_estimated_bytes)
                throw std::length_error("transition byte budget exceeded");

            std::vector<StateId> ids;
            ids.reserve(count);
            const auto allocatedBytes = checkedMultiply(ids.capacity(), sizeof(StateId));
            estimatedBytes = checkedAdd(estimatedBytes, allocatedBytes);
            if (estimatedBytes > options.max_estimated_bytes)
                throw std::length_error("transition byte budget exceeded");
            if (outside) ids.push_back(kOutside);
            ids.insert(ids.end(), hits.begin(), hits.end());
            const auto index = checkedSize(checkedAdd(checkedMultiply(state, actions), action));
            rows[index].ids = std::move(ids);
            statistics.stored_edges = totalEdges;
        }
    }
    statistics.processed_pairs = pairs;
    statistics.abstraction_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    return detail::makeExplicitTransitionSystem(states, actions, guarantee, std::move(rows), statistics);
}

// Input: grid, inputs, transitions: shared immutable handles; metadata: matching model description.
// Output: Validated ModelBundle retaining shared ownership of the three views and owning its metadata.
// Throws: Bundle consistency or checked-count validation errors propagate.
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid> grid,
    std::shared_ptr<const IInputCatalog> inputs, std::shared_ptr<const ITransitionSystem> transitions,
    ModelMetadata metadata) {
    ModelBundle result{std::move(grid), std::move(inputs), std::move(transitions), std::move(metadata)};
    validateBundle(result);
    return result;
}
}
