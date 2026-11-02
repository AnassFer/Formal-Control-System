# Frozen C++ contract 1.0.0

The compilable authority is `include/symctrl/contracts/`; all symbols below are in namespace `symctrl`. C++17, immutable public views, runtime dimensions and standard-library ownership. Shared headers include no private implementation header. All factory definitions remain throwing stubs until their owners implement them on feature branches.

## Values, validation and errors

`StateId`/`InputId` are uint64_t; `Vector=std::vector<double>`, row-major `Matrix=std::vector<Vector>`, `GridIndex=std::vector<uint32_t>`. `Box{Vector lo,hi}`, `GridSpec{Box domain,GridIndex cells_per_dimension}`. Internal states are 1..M; `kOutside=0`. Inputs are 0..A-1 and are distinct from physical vector components. Dimensions must agree and all coordinates must be finite. Degenerate closed boxes are legal. Empty lo/hi vectors describe only the unique zero-dimensional disturbance value. A grid has positive dimension, strictly positive finite widths and nonzero counts. Count/index/offset arithmetic and byte estimates must be checked before allocation.

| Exception | Meaning |
|---|---|
| `std::invalid_argument` | Shape mismatch, nonfinite caller coordinate, invalid box/grid/options/null required handle or inconsistent bundle metadata |
| `std::out_of_range` | Invalid state/action/index/memory, OUT geometry, OUT refinement or missing policy entry |
| `std::overflow_error` | Integer count/offset/byte conversion overflow or an unrepresentable computed numerical result |
| `std::length_error` | Configured pair/edge/storage budget exceeded, or a checked container max_size limit |
| `UnsupportedCapability : std::logic_error` | Missing analytic monotonicity/derivative capability or requested validated arithmetic unavailable |
| `NotImplemented : std::logic_error` | Dedicated missing implementation, the ONLY feature-skip trigger |

Allocation may still propagate `std::bad_alloc`; never pretend that is a successful build or skip. Bootstrap stubs throw `NotImplemented` before doing any feature validation. The error table specifies future implemented behaviour. Shared representation/shape helpers already enforce their documented preconditions.

`validateVector(const Vector&,size_t)`, `validateBox(const Box&,bool allow_empty_disturbance=false)`, `validateGridSpec(const GridSpec&)->StateId`, `validateMatrix(const Matrix&,size_t rows,size_t columns)` and `validateDerivativeBounds(const DerivativeBounds&,size_t n,size_t q)` are complete shape/count checks. Matrix validators require nonnegative finite bounds. `checkedAdd(uint64_t,uint64_t)`, `checkedMultiply(uint64_t,uint64_t)` and `checkedSize(uint64_t)` throw on overflow. They do not discretise a grid or bound dynamics. M+1 must fit IDs/container-size conversion; concrete allocations must also check byte sizes/max_size. `toString(NumericGuarantee)` and `toString(Provenance)` return the frozen manifest spellings and reject invalid enums.

## State and input views

All operations are const. Virtual destructors are public. `noexcept` metadata getters allocate nothing.

| `IStateGrid` signature | Meaning |
|---|---|
| `size_t stateDimension() const noexcept` | Positive n |
| `const GridSpec& spec() const noexcept` | Immutable domain/counts; reference lives with grid |
| `StateId internalStateCount() const noexcept` | M excluding OUT |
| `StateId locate(const Vector& x) const` | Exactly n finite coordinates; outside domain returns 0 |
| `Box cellBounds(StateId state) const` | Owned copy of closed geometry for 1..M; OUT is an error |
| `GridIndex indices(StateId state) const` | Owned n-dimensional index; internal states only |
| `StateId fromIndices(const GridIndex& index) const` | Exactly n indices, each below its count |
| `std::vector<StateId> intersectingInternalCells(const Box& closed_box) const` | Sorted unique internal IDs only; box may be wholly outside or a point |

Dimension 0 varies fastest: `stride[0]=1`, `stride[d]=product(count[j],j<d)`, `id=1+sum(index[d]*stride[d])`. The inverse repeatedly takes `remainder%count[d]`, then integer-divides remainder by count[d], starting with id-1. Production encode/decode is pending; tests contain independent literal orderings and future round trips.

Point ownership uses `[lo,hi)` except the final interval includes the outer upper endpoint. Internal faces go to the higher index in each coordinate; exact domain outer bounds remain internal. Any coordinate strictly outside returns OUT; no epsilon/clamping changes ownership. `cellBounds` uses closed intervals regardless of ownership. Closed intersections can return both adjacent cells at a face. B separately adds OUT for outer-boundary closure contact; the grid never returns it from intersection.

`IInputCatalog` exposes `size_t inputDimension() const noexcept`, `InputId inputCount() const noexcept` and `const Vector& value(InputId input) const`. Both dimensions/counts are positive. The vector reference remains valid for the immutable catalogue lifetime. Explicit catalogues preserve caller order, including duplicate physical vectors as distinct IDs. Cartesian axes preserve their supplied order and dimension 0 varies fastest; each axis is nonempty, including singleton axes. No singleton denominator is used. All values are finite.

## Dynamics and reachability

`IDynamics` getters `stateDimension()`, `inputDimension()`, `disturbanceDimension()` are `size_t`, const noexcept. `Vector step(const Vector& x,const Vector& u,const Vector& w) const` returns n owned finite components; input dimensions are n,m,q. It does not own caller vectors. Thermostat uses n=m=q=1, physical u in {0,1}; building uses n=m=N>=3, q=0 and fixed benchmark selected controls. Invalid model inputs/shapes fail explicitly.

Model-specific capabilities are frozen:

```cpp
struct DerivativeBounds { Matrix Dx; Matrix Dw; };
bool IDynamics::cooperativeOn(const Box& states, const Vector& input,
                             const Box& disturbances) const;
DerivativeBounds IDynamics::absoluteJacobianBounds(const Box& states, const Vector& input,
                                                   const Box& disturbances) const;
```

These are virtual members. CooperativeOn is true only when nondecreasing in every state/disturbance coordinate over the entire states×disturbances box for that fixed input, with analytic model documentation. False means the monotone backend must reject the request with UnsupportedCapability. Bounds are owned row-major n×n and n×q arrays, componentwise absolute upper bounds over the entire same product box. For q=0, Dw has n empty rows, not zero rows. Missing bounds throw UnsupportedCapability; malformed returned bounds fail invalid_argument. No centre-point fallback. See [source-map.md](source-map.md) for equations/proofs.

`IReachability` has the same three size getters, `NumericGuarantee numericGuarantee() const noexcept` and `Box enclose(const Box& cell,const Vector& input,const Box& disturbance_box) const`. It returns an owned n-dimensional closed enclosure covering every closed-cell point and every allowed disturbance. The backend retains a shared_ptr<const IDynamics>; caller lifetimes are independent. Method enum: `CooperativeMonotone` or `BoundedDerivative`. `ReachabilityOptions{bool require_validated=false}` demands genuine validated arithmetic when true; it is not an epsilon or a certification label. Default ordinary-double backends advertise `UncertifiedFloat`.

## Graph and successor operations

`SuccessorSet{SuccessorKind kind,std::vector<StateId> ids}` has `Explicit` sorted unique IDs in 0..M, or `AllStates` with empty storage meaning every ID 0..M. A genuinely empty Explicit set is representable for diagnostics, but invalid/incomplete producer output for the complete models here. All inputs are enabled in every state. Do not interpret empty successors as vacuous success or a disabled action.

```cpp
void validateSuccessors(const SuccessorSet&, StateId internal_states);
bool contains(const SuccessorSet&, StateId state, StateId internal_states);
bool nonempty(const SuccessorSet&, StateId internal_states);
void forEachSuccessor(const SuccessorSet&, StateId internal_states,
                      const std::function<void(StateId)>& visit);
```

These shared operations validate representation; malformed sets throw invalid_argument, membership IDs beyond M throw out_of_range. Iteration is ascending, invokes once per mathematical successor, allocates no universal vector and is overflow-safe at the last ID. Visitor exceptions propagate; an empty visitor is invalid. Explicit validation is O(stored edges); AllStates membership/nonemptiness are constant time. Consumers should check nonemptiness before any robust universal condition and use `contains(set,0,M)` for OUT. Iterating AllStates can be expensive by definition.

`ITransitionSystem` exposes `StateId internalStateCount() const noexcept`, `InputId inputCount() const noexcept`, `NumericGuarantee numericGuarantee() const noexcept`, `const BuildStatistics& statistics() const noexcept` and `const SuccessorSet& successors(StateId state,InputId input) const`. Query ranges are state 0..M, action 0..A-1. References remain valid for graph lifetime, including after other queries. Graphs own their stored output and keep no dangling references to build arguments. OUT/action is compact AllStates. Numeric status propagates from reachability; tests cannot upgrade it.

`BuildOptions{uint64_t max_state_input_pairs=1000000,max_stored_edges=10000000,max_estimated_bytes=536870912}` limits all `(M+1)*A` pairs including OUT, stored Explicit IDs and estimated storage. Check arithmetic and limits before large allocation; stop explicitly when a growing edge budget is exceeded. AllStates contributes no stored explicit edge. Avoid dense M×A×M storage. `BuildStatistics{uint64_t processed_pairs=0,stored_edges=0; double abstraction_seconds=0;}` counts complete pair processing including OUT, explicit stored IDs and graph-build wall time excluding export/grid setup. Fixtures leave all statistics zero because no abstraction was run.

## Policy and refinement

`PolicyMemory{uint64_t value=0}`, `PolicyDecision{PolicyMemory next_memory;InputId input;}` and `ConcreteDecision{StateId state;InputId input;Vector physical_input;PolicyMemory next_memory;}` are owned values. `PolicyDecision ISymbolicPolicy::decide(PolicyMemory previous_memory,StateId current_state) const` performs memory update then action selection using the updated memory, matching paper §1.2. The caller supplies psi(-1), default 0, and then passes each returned memory to the next decision. Memoryless policies leave or ignore it. A different initial memory is supplied explicitly by the policy's client/specification.

Future refinement calls grid.locate, rejects OUT, calls policy.decide, validates the selected action through inputs.value, then returns an owned physical vector with state/action/new memory. Missing policy entries, invalid memory and invalid actions fail out_of_range; no invented fallback input. The adapter chooses no disturbance and synthesises nothing.

## Exact public factory boundary

```cpp
// A definitions: src/part_a/*.cpp
std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec);
std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values);
std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector> per_dimension_values);
ConcreteDecision refineDecision(const IStateGrid& grid, const IInputCatalog& inputs,
    const ISymbolicPolicy& policy, const Vector& measured_x, PolicyMemory previous_memory = {});
// B definitions: src/part_b/*.cpp
std::shared_ptr<const IDynamics> makeThermostatDynamics();
std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms);
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics> dynamics,
    ReachabilityMethod method, ReachabilityOptions options = {});
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid& grid,
    const IInputCatalog& inputs, const IReachability& reachability, const Box& disturbance,
    BuildOptions options = {});
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid> grid,
    std::shared_ptr<const IInputCatalog> inputs, std::shared_ptr<const ITransitionSystem> transitions,
    ModelMetadata metadata);
struct ExportOptions { std::uint64_t max_transition_rows = 10000000; };
void exportModelPackage(const ModelBundle& bundle, const std::filesystem::path& directory,
    ExportOptions options = {});
```

Grid/input factories copy/move their value arguments. Reachability retains its dynamics handle. Graph construction consumes const references only for the call and owns its output. Bundle construction retains all three immutable handles, checks consistency and returns metadata by value. Export writes the complete v1 package, honours a row limit including universal rows and fails explicitly on I/O/budget errors; it never silently emits an incomplete package as complete. Diagnostic subsets need separate labelling and are not v1 complete model packages. Export/import algorithms remain unimplemented. `factories.hpp` is the shared declaration authority; no private concrete type is exposed.

## Model bundle and versions

`ModelBundle` contains shared_ptr<const IStateGrid> grid, shared_ptr<const IInputCatalog> inputs, shared_ptr<const ITransitionSystem> transitions, plus owned `ModelMetadata metadata`. Metadata fields are exactly declared in `model_bundle.hpp`: contract/schema strings (1.0.0), model_id, numeric model_parameters map, Provenance, source, n/m/q dimensions, M/A counts, outside_state_id, NumericGuarantee, GridSpec and disturbance Box. `validateBundle(const ModelBundle&)` is a completed trivial consistency check: required handles, supported versions, dimensions/counts/domain agreement and finite metadata. It is not an exhaustive successor check or numerical certification. Treat a published bundle and its metadata as immutable.

The fixture SDK constructs bundles directly with its own tiny tables and validateBundle; it never calls unfinished production factories. Headers/constants currently accept exactly version 1.0.0. Future consumers must at least reject unsupported major versions; compatible minor additions require explicit negotiation, while incompatible signatures/ID/boundary/transition meaning require coordinated versioning. [part1-to-part2.md](part1-to-part2.md) freezes serialization and downstream task/policy semantics.
