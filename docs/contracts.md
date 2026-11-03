# Current general public contract, version 1.0.0

The compilable authority is include/symctrl/contracts/. All symbols are in namespace symctrl, using C++17 and runtime dimensions. This document describes the model-independent interfaces for Part 1 and the model/policy base for Part 2. [The current scope](scope-correction.md) keeps all stages general. Legacy example declarations remain unchanged in the v1 code, outside this core description.

## Values and validation

StateId/InputId are uint64_t. Vector is vector<double>; Matrix is vector<Vector> in row-major order; GridIndex is vector<uint32_t>. Box contains owned lo/hi vectors; GridSpec contains a Box domain and per-dimension cell counts. Internal states are 1..M, kOutside=0, actions are 0..A-1.

Coordinates are finite and dimensions must match. Closed point boxes are valid. A zero-dimensional box/vector is only the unique absent disturbance. Grids have positive dimension/widths/counts. Check count, offset, ID, container-size and byte arithmetic before allocation.

| Error | Meaning |
|---|---|
| invalid_argument | Invalid shape/value/box/grid/options/required handle or inconsistent metadata |
| out_of_range | Invalid state/action/index/policy entry, OUT geometry or OUT refinement |
| overflow_error | Arithmetic/conversion overflow or unrepresentable computed result |
| length_error | Configured processing/storage budget or checked container limit exceeded |
| UnsupportedCapability | Required monotonicity/bounds/validated arithmetic unavailable |
| NotImplemented | Dedicated missing feature; the only allowed feature-skip trigger |

Allocation errors remain errors; no fake success or arbitrary-exception skip. Current production stubs throw NotImplemented before feature validation. Shared validation helpers are implemented.

Helpers include validateVector, validateBox, validateGridSpec, validateMatrix, validateDerivativeBounds, checkedAdd, checkedMultiply and checkedSize. Matrix validation requires nonnegative finite entries and correct n×n/n×q shapes. Count validation does not implement production indexing/discretisation.

## Grid and input views

IStateGrid has const operations stateDimension(), spec(), internalStateCount(), locate(x), cellBounds(id), indices(id), fromIndices(index) and intersectingInternalCells(box). Metadata getters are noexcept. spec returns a const reference with grid lifetime; bounds/indices/intersection lists are owned values.

Dimension 0 varies fastest: stride[0]=1, stride[d]=product(count[j],j<d), id=1+sum(index[d]*stride[d]). Invert id-1 by remainder/modulo counts in increasing dimension order. Check IDs/indices and arithmetic.

Point intervals are half-open except the final domain endpoint. Internal faces go to higher indices; outside coordinates return OUT; reject nonfinite inputs and do not apply a global ownership epsilon. cellBounds returns closed geometry for internal IDs only. Closed intersections are sorted unique internal IDs; B adds OUT separately.

IInputCatalog exposes inputDimension(), inputCount() and const Vector& value(InputId). Dimension/count are positive and refs remain valid for catalogue lifetime. Explicit catalogues preserve caller order; Cartesian axes preserve supplied order with dimension 0 fastest. Singleton axes are valid; invalid actions/shapes/nonfinite values fail explicitly.

## Supplied dynamics and reachability

IDynamics exposes n/m/q dimensions and Vector step(x,u,w) const. Inputs have exactly those dimensions and output is an owned finite n-vector. Model implementations supply mathematical capabilities through:

```cpp
bool cooperativeOn(const Box& states, const Vector& input,
                   const Box& disturbances) const;
DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                        const Box& disturbances) const;
```

These are virtual members. Cooperative truth requires an analytic nondecreasing argument over the requested state/disturbance boxes for the fixed input. False must make the monotone backend reject the request. DerivativeBounds contains owned Dx/Dw absolute upper-bound matrices valid over those whole boxes, not just at a centre. With q=0, Dw has n empty rows. Missing capabilities throw UnsupportedCapability; malformed bounds are invalid_argument.

IReachability exposes matching dimensions, numericGuarantee() and Box enclose(cell,input,W) const. It encloses every closed-cell state/disturbance and returns an owned n-Box. Factory-created backends retain shared_ptr<const IDynamics>. Methods are CooperativeMonotone and BoundedDerivative. ReachabilityOptions.require_validated defaults false and must reject an unavailable certification backend when true. Ordinary doubles are UncertifiedFloat.

## Graph and successor semantics

SuccessorSet has kind Explicit with sorted unique IDs in 0..M, or AllStates with empty storage meaning all 0..M. Empty Explicit is representable for diagnostics but invalid/incomplete producer output for current complete models. All actions are enabled everywhere; disabled-input semantics require an agreed extension.

```cpp
void validateSuccessors(const SuccessorSet&, StateId internal_states);
bool contains(const SuccessorSet&, StateId state, StateId internal_states);
bool nonempty(const SuccessorSet&, StateId internal_states);
void forEachSuccessor(const SuccessorSet&, StateId internal_states,
                      const std::function<void(StateId)>& visit);
```

Helpers validate representation. Invalid membership IDs throw out_of_range. Iteration is ascending, allocates no universal vector and safely stops at the final ID. Visitor exceptions propagate. AllStates membership/nonemptiness are constant-time. Check nonemptiness before universally quantified control conditions; detect OUT with contains(set,0,M).

ITransitionSystem exposes internalStateCount(), inputCount(), numericGuarantee(), statistics() and const SuccessorSet& successors(state,input) const. Queries accept 0..M and 0..A-1. Returned refs remain valid across other queries for graph lifetime. Graphs own their output independently of build inputs. OUT/action returns compact AllStates.

BuildOptions limits (M+1)*A pairs, stored explicit successor IDs and estimated bytes; current defaults are 1,000,000 pairs, 10,000,000 IDs and 536,870,912 bytes. Check limits before large allocation. BuildStatistics stores processed_pairs, stored_edges and abstraction_seconds; universal rows add no stored explicit IDs. Fixtures report zero generation statistics because no production algorithm ran.

## Policy execution and future synthesis base

PolicyMemory owns uint64 value, default 0. ISymbolicPolicy::decide(previous_memory,current_state) const returns PolicyDecision{next_memory,input}; memory update precedes action choice. Caller feeds the returned memory into the next call. Missing entries/invalid memory fail explicitly. ConcreteDecision owns state, action, physical vector and next memory.

A's adapter locates a measured state, rejects OUT, calls the supplied policy, validates its action through the catalogue and returns an owned vector. It does not select disturbances or synthesise a controller.

Part 2 later consumes a graph and separate specification to construct policies. Exact public synthesis/specification/result APIs remain to be agreed; ISymbolicPolicy alone is not an implemented solver. Keep that future layer model-independent.

## Existing general factories

```cpp
std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec);
std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values);
std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector> per_dimension_values);
ConcreteDecision refineDecision(const IStateGrid& grid, const IInputCatalog& inputs,
    const ISymbolicPolicy& policy, const Vector& measured_x, PolicyMemory previous_memory = {});
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics> dynamics,
    ReachabilityMethod method, ReachabilityOptions options = {});
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid& grid,
    const IInputCatalog& inputs, const IReachability& reachability, const Box& disturbance,
    BuildOptions options = {});
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid> grid,
    std::shared_ptr<const IInputCatalog> inputs, std::shared_ptr<const ITransitionSystem> transitions,
    ModelMetadata metadata);
void exportModelPackage(const ModelBundle& bundle, const std::filesystem::path& directory,
    ExportOptions options = {});
```

All production definitions are still stubs. A owns its factories/refinement; B owns reachability/build/bundle/export. Value settings become owned configuration. Backends retain dynamics; graph build borrows its views only for the call and owns the output. Bundle handles retain view lifetimes. Export must write the full versioned package and fail explicitly on I/O/row-budget errors; no exporter is implemented now.

## Bundle and versions

ModelBundle contains shared_ptr<const IStateGrid>, shared_ptr<const IInputCatalog>, shared_ptr<const ITransitionSystem> and owned ModelMetadata. Metadata includes versions, model identifier/parameters, provenance/source, n/m/q, M/A, OUT, numeric guarantee, grid and disturbance. validateBundle is a completed trivial consistency helper, not exhaustive graph/numerical certification. Treat published metadata/views as immutable.

Current helpers/schema accept 1.0.0. Consumers reject unsupported major versions; no addition silently changes IDs, boundaries or transition meaning. Incompatible public changes require common versioning. No API/schema is changed by this documentation revision. See [the handoff](part1-to-part2.md) for the current file route.
