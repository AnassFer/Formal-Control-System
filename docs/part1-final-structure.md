# Intended general Part 1 structure and Part 2 base

This is the model-independent blueprint. Contracts/helpers/fixtures exist; production algorithms remain stubs. A/B implement paper Section 1, while the reserved Part 2 base later implements general synthesis from Section 2. A mobile robot is an example after both stages. See [the scope audit](scope-correction.md).

## Frozen public boundary

All headers below are under include/symctrl/contracts/ and belong to symctrl_contracts. Exact existing signatures, shapes, ownership and errors are in [contracts.md](contracts.md).

| Header | Responsibility | Main clients |
|---|---|---|
| types.hpp | State/input IDs, vectors, row-major matrices, boxes, grid settings, errors/status and validation declarations | All modules |
| grid.hpp | IStateGrid: locate, bounds, IDs/indices and closed internal intersections | A implements; B and later consumers query |
| inputs.hpp | IInputCatalog: immutable physical input vectors by action ID | A implements; B/refinement/Part 2 consume |
| dynamics.hpp | IDynamics: step, dimensions and full-box mathematical capabilities | Supplied model adapters and B backends |
| reachability.hpp | IReachability: full-cell/disturbance enclosure and numeric status | B implements; graph builder consumes |
| transitions.hpp | ITransitionSystem, successor representation/helpers and build budgets/statistics | B produces; Part 2 consumes |
| policy.hpp | ISymbolicPolicy, memory/decision values and owned concrete decision | Part 2 supplies policies; A refines |
| model_bundle.hpp | Owned immutable views plus matching metadata | Producer and consumers |
| factories.hpp | Public creation/refinement/graph/export boundaries | Public clients and integration |

The table describes the general boundary. Legacy example-specific declarations in the current v1 tree are outside this intended core and unchanged by the documentation-only revision. No planned synthesis declaration is presented as implemented.

## Existing common helpers

| Source under src/common/ | Target | Responsibility/status |
|---|---|---|
| validation.cpp | symctrl_common | Complete finite shape, checked count and nonnegative matrix-bound validation |
| successors.cpp | symctrl_common | Complete sorted/unique representation checks and universal-set-safe utilities |
| model_bundle.cpp | symctrl_common | Complete trivial handle/metadata consistency; no generation/certification |
| CMakeLists.txt | symctrl_common | Explicit source list and contracts dependency |

All common helpers remain frozen; no discretisation, physical update or synthesis belongs there.

## A modules: general discretisation and refinement

| Source under src/part_a/ | Public operation/interface | Implementation responsibility |
|---|---|---|
| uniform_grid.cpp | makeUniformGrid(GridSpec) -> shared_ptr<const IStateGrid> | Generic n-dimensional IDs/indices, geometry, point ownership and closed intersections |
| input_catalog.cpp | makeInputCatalog(vector<Vector>), makeCartesianInputs(vector<Vector>) -> shared_ptr<const IInputCatalog> | Physical vector ordering, singleton axes and checked enumeration |
| policy_adapter.cpp | refineDecision(grid,inputs,policy,measured_x,previous_memory) -> ConcreteDecision | Locate, reject OUT, apply policy, validate action and return owned vector/new memory |
| factories.cpp | Reserved coordination; existing symbols are defined in their module files | No duplicate definitions |
| CMakeLists.txt | symctrl_part_a | Owner-local source extension; common/contracts only |

Inputs have checked dimensions/counts/finite coordinates. Grids own configuration; catalogue refs live with their immutable object; concrete vectors are owned values. Standard invalid_argument/out_of_range/overflow errors follow contracts.md. Factories currently throw NotImplemented.

A's optional private headers belong in include/symctrl/part_a/. Tests/notes belong in tests/part_a/ and docs/part_a/. Tests use real A without B. Check indexing in multiple dimensions, boundaries/OUT, closed-face contact, input order, invalid arguments/overflow and finite-memory policy refinement.

## B modules: general reachability and model production

| Source under src/part_b/ | Public operation/interface | Implementation responsibility |
|---|---|---|
| monotone_reachability.cpp | Private IReachability selected by CooperativeMonotone | Analytically justified endpoint enclosure through supplied IDynamics |
| derivative_reachability.cpp | Private IReachability selected by BoundedDerivative | Full-box absolute n×n/n×q bounds and matrix-vector radius |
| factories.cpp | makeReachability(shared dynamics,method,options) -> shared_ptr<const IReachability> | Retain dynamics; explicit method/capability handling |
| explicit_transition_system.cpp | Private ITransitionSystem | Owned sparse immutable output, stable query refs and compact universal rows |
| model_builder.cpp | buildSymbolicModel(grid,inputs,reachability,W,options); assembleModelBundle(grid,inputs,graph,metadata) | General enclosure-to-cell transitions, closure/OUT, budgets and bundle consistency |
| model_export.cpp | exportModelPackage(bundle,directory,options) | Complete generic v1 package; no incomplete subset masquerading as a model |
| CMakeLists.txt | symctrl_part_b | Owner-local source extension; common/contracts only |

Backends retain immutable dynamics; build arguments are borrowed only during the call; the graph owns resulting data. Bundle handles retain grid/catalogue/graph lifetimes. Unsupported capabilities fail explicitly, budget exhaustion is an error, and output preserves the backend's numeric status. All production implementations remain missing.

B's optional private headers belong in include/symctrl/part_b/. Tests/notes belong in tests/part_b/ and docs/part_b/. Use fake A views and independent generic dynamics/enclosure fixtures to verify dimensions, q=0, mixed signs, unsupported capabilities, invalid matrices, boundaries/OUT, valid successor representation, ownership and limits.

## Shared fixtures, clients and integration

| Path | Target/purpose | Status/dependencies |
|---|---|---|
| fixtures/support/fixture.hpp and fixture.cpp | symctrl_fixture_support; tiny literal grid/input/graph views | Working fixture data; common/contracts only |
| fixtures/part1_v1/ | Complete fixed model packages/specification examples | Existing files; no algorithm generation |
| schemas/part1-model-v1.schema.json | Manifest structure | Existing schema; cross-file semantics in handoff docs |
| examples/part2_fixture_consumer.cpp | part2_fixture_consumer | Working query example, no A/B link or synthesis |
| examples/standalone/ | external_fixture_client | Working separate CMake consumer |
| tests/contracts/ | contract_tests | Shared representation/validation checks |
| tests/part2_handoff/ | part2_handoff_tests and output comparison | Working fixture-interface checks |
| tests/integration/ | part1_integration_tests | Original cross-part assertions, pending real implementations |
| tests/support/ | symctrl_test_support | Release-safe explicit checks and independent expectations |
| apps/ | symctrl_cli | Existing legacy skeleton; a general CLI is future coordinated work |

Root CMake adds owner-local CMake entry points. No source globbing or private-header dependency is required. Shared fixtures/tests do not change to fit faulty production output. Current test coverage is not a full generic-controller acceptance suite.

## Reserved Part 2 base and later example

Existing paths include include/symctrl/part2/, src/part2/CMakeLists.txt, tests/part2/CMakeLists.txt and docs/part2/. SYMCTRL_BUILD_PART2 enables symctrl_part2_workspace, an INTERFACE target with no compiled algorithm.

Part 2 inputs are a generic graph/bundle and a separate specification. Its eventual output is a policy implementing ISymbolicPolicy plus any internal synthesis result. Specification types, exact synthesis/result factories and validation need common agreement before public additions. Safety, reachability and any needed finite-automaton support stay general. Add implementation/tests through the owned local CMake files; do not modify the fixed consumer as an algorithm workspace.

A later mobile-robot adapter/demonstration uses the completed pipeline. Keep robot parameters, region labels and periodic-heading handling outside generic solvers. No robot file/factory or generic CLI option is claimed to exist now.

```text
Supplied model -> A grid/inputs -> B enclosure/graph -> immutable bundle/package
Bundle + separate specification -> Part 2 synthesis -> symbolic policy
Measured state + policy + previous memory -> A refinement -> physical input
```

This documentation revision does not change C++, schema, CMake, tests, Git refs or implementation status.
