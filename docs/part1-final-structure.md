# Final intended Part 1 structure: v1 blueprint

This is the completed **design**, not a completed producer. Public contracts and trivial common helpers are implemented; all production features below remain stubs/reserved. Concrete production classes will stay translation-unit-private in the named .cpp modules. The owned include directories permit optional future private headers; no downstream client needs them. Paths below are repository-relative, and no downstream-facing filename/signature is deferred.

All exact interface methods, value fields, preconditions and error classes are frozen in [contracts.md](contracts.md) and the corresponding headers. In the module tables, implementing an interface inherits every signature, dimension, ID rule and lifetime from that contract. Factory signatures are reproduced in full below. Standard exceptions apply as documented there; bootstrap production definitions throw NotImplemented unconditionally. Shared code never includes a private A/B header.

## Public declaration modules (all frozen shared)

| Exact header under `include/symctrl/contracts/` | Target / status | Responsibility, inputs → outputs and lifetime | Dependencies / later implementation / checks |
|---|---|---|---|
| `types.hpp` | symctrl_contracts / contract | IDs, owned Vector/row-major Matrix/Box/GridSpec, errors, numeric/provenance enums; shape/count checks declared | stdlib only; representation stays frozen; contracts.shapes/counts/metadata |
| `grid.hpp` | symctrl_contracts / contract | IStateGrid, §1.1.1; points/IDs/indices/closed Boxes → IDs, owned bounds/indices/intersections; spec ref lives with grid | types; A implements it, B/Part 2 consume; A indexing/geometry/invalid and integration |
| `inputs.hpp` | symctrl_contracts / contract | IInputCatalog, §1.1.1; action ID → immutable physical m-vector ref; n/a OUT; ref lives with catalogue | types; A implements, B/adapter/Part 2 consume; A inputs and handoff |
| `dynamics.hpp` | symctrl_contracts / contract | IDynamics and DerivativeBounds, §1.1.2–3; n,m,q values/boxes → owned n-vector or n×n/n×q absolute bounds; capability declaration | types; B implements, enclosure backends consume; B thermostat/affine/capabilities/building |
| `reachability.hpp` | symctrl_contracts / contract | IReachability, methods/options, §1.1.3; closed n-cell, m-input, q-disturbance → owned enclosing n-Box plus numerical status | types; B retains immutable dynamics; graph consumes; B enclosure/capability tests |
| `transitions.hpp` | symctrl_contracts / contract | ITransitionSystem, SuccessorSet, checked representation utilities/options/stats, §1.1.2; state/action → stable successor ref | types; B owns graph output, Part 2 consumes; contracts.successors, B graph/limits, integration |
| `policy.hpp` | symctrl_contracts / contract | ISymbolicPolicy, PolicyMemory/Decision/ConcreteDecision, §1.2; previous memory/state → new memory/action; concrete vector is owned | types; A adapter consumes externally supplied/Part 2 policy; A policy and integration |
| `model_bundle.hpp` | symctrl_contracts / contract | ModelMetadata/ModelBundle and validateBundle; three shared immutable handles + matching metadata; copies retain ownership | grid/inputs/transitions; B assembles future output, fixtures construct directly, Part 2 consumes; handoff.consistency |
| `factories.hpp` | symctrl_contracts / contract | Exact A/B producer/refinement/export declarations and ExportOptions, §1 as applicable | all public interfaces + filesystem; no definitions/private classes; owner factories/integration compile against this boundary |

For all metadata getters, return type is size_t/StateId/InputId/NumericGuarantee as declared and operations are const noexcept. Query values are checked, never silently clamped. Bundle metadata must agree with all three views; the container itself is not an abstraction or synthesis algorithm.

## Complete common implementation modules (frozen shared)

| Exact path | Target / status | Inputs → outputs, errors and dependencies | Future change / checks |
|---|---|---|---|
| `src/common/validation.cpp` | symctrl_common / complete helper | vectors/boxes/grid counts/matrices → validation or checked scalar counts; finite shapes, nonnegative derivative bounds, invalid_argument/overflow_error; contracts only | no production encoding/physics added; contracts.shapes/counts/metadata |
| `src/common/successors.cpp` | symctrl_common / complete helper | raw SuccessorSet+M(+ID/visitor) → validation/membership/nonempty/ascending callbacks, no universal allocation; malformed/invalid arguments fail | representation remains frozen; contracts.successors and handoff |
| `src/common/model_bundle.cpp` | symctrl_common / complete helper | ModelBundle → null/version/count/shape/domain/numeric consistency checks; invalid_argument on disagreement; no graph traversal | no generation/certification added; handoff.consistency |
| `src/common/CMakeLists.txt` | symctrl_common / complete build | explicit common source list → static library, public symctrl_contracts dependency | frozen; independent SDK configure/build |

## A production modules (all Part A owned)

All factory arguments by value become owned copied/moved configuration; public return handles are shared_ptr<const interface>. The adapter takes const references only during a call and returns a value. No A module depends on B or fixture support.

| Exact source | Target / status | Interface/signature, responsibility and paper section | Inputs/outputs, downstream, later change and checks |
|---|---|---|---|
| `src/part_a/uniform_grid.cpp` | symctrl_part_a / throwing stub | `std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec)`; uniform grid, indices, geometry, q(x), closed intersections, §1.1.1 | n positive domain/counts → immutable IStateGrid; checked count/ID/shape errors; used by B, refinement and Part 2; replace stub with private grid class; A indexing/geometry/invalid, integration |
| `src/part_a/input_catalog.cpp` | symctrl_part_a / throwing stubs | `std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values)` and `makeCartesianInputs(std::vector<Vector> per_dimension_values)`; finite input enumeration/p(action), §1.1.1 | A m-vectors or m ordered nonempty axes → immutable catalogue; dimensions/order/singletons/ID/overflow checks; B/refinement/Part 2 consume; replace stubs; A inputs |
| `src/part_a/policy_adapter.cpp` | symctrl_part_a / throwing stub | `ConcreteDecision refineDecision(const IStateGrid&,const IInputCatalog&,const ISymbolicPolicy&,const Vector&,PolicyMemory={})`; refinement, §1.2 | measured n-vector and psi(-1)/previous memory → state/action/owned physical m-vector/new memory; reject OUT, invalid actions/missing policy; CLI and external runtime consume; replace stub; A policy and integration |
| `src/part_a/factories.cpp` | symctrl_part_a / reserved | includes frozen factory declarations; A-only private dispatch coordination if later needed | no duplicate definitions; existing symbols are defined in corresponding module files; compile/build checks |
| `src/part_a/CMakeLists.txt` | symctrl_part_a / complete build | explicit four-source list; public common dependency, private warnings | owner can add private files without root edits; isolated A build |
| `include/symctrl/part_a/README.md` | no target / reserved private include tree | reserves owner-only internal headers, no public ABI | consumers use contracts; optional internal additions owned by A |
| `tests/part_a/part_a.cpp`, `tests/part_a/CMakeLists.txt` | part_a_tests / assertions written, pending features | indexing/geometry/inputs/invalid/policy cases; fixed memory policy fixture | real A only + common/test helpers; NotImplemented alone skips; owner extends local source list/tests |
| `docs/part_a/README.md` | documentation / reserved stream notes | A scope, dependencies and independent gate | A records subsequent feature progress here |

## B production modules (all Part B owned)

Only contracts/common are production dependencies. IDynamics owns no caller buffers. Reachability retains shared immutable dynamics. The graph owns its output after its reference arguments go away. Bundle assembly retains all three handles. Export receives a const bundle; the path/options are call inputs, no references are retained.

| Exact source | Target / status | Interface/signature, responsibility and paper section | Inputs/outputs, downstream, later change and checks |
|---|---|---|---|
| `src/part_b/thermostat_dynamics.cpp` | symctrl_part_b / throwing stub | `std::shared_ptr<const IDynamics> makeThermostatDynamics()`; fixed thermostat physics/capabilities, §1.1.2–3 | n=m=q=1, selected u=0/1 and documented constants; returns immutable IDynamics, rejects invalid shapes/values; backend/CLI consume; replace stub; six thermostat intervals/analytic steps |
| `src/part_b/ring_building_dynamics.cpp` | symctrl_part_b / throwing stub | `std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms)`; fixed periodic physics/capabilities, slides 14–16 under §1.1.2–3 | N>=3 → n=m=N,q=0 model; empty w, absolute derivatives, index correctness; backend consumes; replace stub; B building literal N=3/4 checks |
| `src/part_b/monotone_reachability.cpp` | symctrl_part_b / reserved translation unit | private IReachability implementation selected by makeReachability CooperativeMonotone, §1.1.3 | retained dynamics + closed n/q boxes/m-input → owned n-enclosure; analytic cooperative capability required; no fallback; graph consumes; implement later; B thermostat/affine/capabilities and integration |
| `src/part_b/derivative_reachability.cpp` | symctrl_part_b / reserved translation unit | private IReachability implementation selected by BoundedDerivative, §1.1.3 | retained dynamics + full-box n×n/n×q absolute bounds → centre/radius enclosure; shape/nonnegative/unsupported errors; graph consumes; implement later; mixed-sign affine and malformed-bound tests |
| `src/part_b/explicit_transition_system.cpp` | symctrl_part_b / reserved translation unit | private ITransitionSystem storage, §1.1.2 | checked sparse Explicit sets, shared compact universal row → immutable owned graph; stable references; Part 2 consumes; implement later; B graph validity/lifetime and handoff compatibility |
| `src/part_b/model_builder.cpp` | symctrl_part_b / throwing stubs | `std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid&,const IInputCatalog&,const IReachability&,const Box&,BuildOptions={})`; `ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid>,std::shared_ptr<const IInputCatalog>,std::shared_ptr<const ITransitionSystem>,ModelMetadata)` | compatible views/W/options → graph with closure/OUT/count/budget checks, or owned matching bundle; §1.1.2 producer; consumer/export/CLI downstream; replace stubs; B graph/boundaries/limits/export and real integration |
| `src/part_b/model_export.cpp` | symctrl_part_b / throwing stub | `void exportModelPackage(const ModelBundle&,const std::filesystem::path&,ExportOptions={})`; versioned complete-package handoff | immutable bundle + directory/max rows → four files, no synthetic model/task; count/row/I/O errors explicit, output not included in abstraction timing; implement later; B export exact CSV assertions/schema |
| `src/part_b/factories.cpp` | symctrl_part_b / throwing stub | `std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics>,ReachabilityMethod,ReachabilityOptions={})`; §1.1.3 dispatch | nonnull retained dynamics/method/require_validated → immutable backend; unsupported certification errors; no duplicate model/grid definitions; replace stub; B enclosure tests |
| `src/part_b/CMakeLists.txt` | symctrl_part_b / complete build | explicit eight-source list, public common/private warnings | owner can add sources without root edits; isolated B build |
| `include/symctrl/part_b/README.md` | no target / reserved private include tree | owner-only optional internal headers | no consumer dependency |
| `tests/part_b/part_b.cpp`, `tests/part_b/CMakeLists.txt` | part_b_tests / assertions written, pending features | thermostat/affine/capabilities/graph/boundaries/limits/building/export | real B with literal fake A tables and analytic affine test dynamics; no A library; owner extends local tests |
| `docs/part_b/README.md` | documentation / reserved stream notes | B scope/independent gates | owner records subsequent progress here |

## Fixture, client and shared verification modules

| Exact path(s) | Owner / target / status | Data, errors, dependencies and future checks |
|---|---|---|
| `fixtures/support/fixture.hpp`, `fixture.cpp`, `CMakeLists.txt` | frozen / symctrl_fixture_support / fixture | `ModelBundle fixtures::thermostat()` and `nondeterministicChoice()` expose literal 3-cell/2-input table adapters implementing IStateGrid/IInputCatalog/ITransitionSystem; no production factory or general discretisation/physics. Own records; bounds/input/graph invalid IDs fail; common only; B-isolation/handoff checks |
| `fixtures/part1_v1/thermostat/{manifest.json,cells.csv,inputs.csv,transitions.csv,expected-output.txt}` | frozen / fixed package / fixture | complete six-row oracle plus OUT; closed geometry; schema 1.0.0 and uncertified fixture provenance; handoff.packages/consumer |
| `fixtures/part1_v1/nondeterministic_choice/{manifest.json,cells.csv,inputs.csv,transitions.csv,specification.json}` | frozen / fixed model+separate task / fixture | manual robust-choice trap; no synthesis computation; handoff.choice/packages |
| `schemas/part1-model-v1.schema.json` | frozen / serialization contract | exact manifest structure; CSV rules/cross-file checks in part1-to-part2.md; optional fixture checker and JSON Schema validation |
| `tests/support/test.hpp`, `oracle.hpp` | frozen / symctrl_test_support / test helpers | reusable explicit Release-safe checks, exception harness and independent literal thermostat/manual-policy fixtures; no assertions erased by NDEBUG; owner test dependencies |
| `tests/contracts/contracts.cpp`, `CMakeLists.txt` | frozen / contract_tests / complete foundation | shapes, counts/overflow, successor representations, metadata/interface compile checks; common only |
| `tests/integration/part1.cpp`, `CMakeLists.txt` | frozen / part1_integration_tests / pending real features | real A factories + B model/backends/graph; all six successors, OUT, supplied refinement, two physical steps, bounded deterministic sequence membership and method comparison; public headers only; no fixture library |
| `tests/part2_handoff/handoff.cpp`, `consumer.cmake`, `CMakeLists.txt` | frozen / part2_handoff_tests / complete fixture checks | public views, owned lifetimes/metadata, literal CSV comparisons and byte-exact consumer output; common+fixture only, never A/B |
| `examples/part2_fixture_consumer.cpp`, `examples/CMakeLists.txt` | frozen / part2_fixture_consumer / complete fixture client | owned fixture bundle → deterministic metadata/query output, including universal iteration/OUT; no generation/search/synthesis; common+fixture only |
| `examples/standalone/{CMakeLists.txt,client.cpp}` | frozen / external_fixture_client / complete SDK client | tested separate CMake add_subdirectory consumption with production OFF; no A/B link |
| `examples/thermostat.json`, `building-n3.json` | frozen / no target / specification fixtures | final small/default and fixed benchmark configs; no config loader implemented |
| `apps/main.cpp`, `apps/CMakeLists.txt` | frozen / symctrl_cli / orchestration skeleton | documented commands call public production factories; current exit 2 on NotImplemented, invalid usage/error 1; future calls print config/numeric/stats and supplied-policy steps. Hand table is not synthesis. CLI links both producer libraries, not fixtures |

## Build, workflow and reserved Part 2 modules

Root `CMakeLists.txt` freezes the contracts/common/A/B/CLI/fixture/test graph; `CMakePresets.json` provides dev/release/sdk/sanitize. Warnings and optional sanitizers are shared build settings. `.github/workflows/ci.yml` runs GCC/Clang Linux bootstrap checks and a separate SDK build; a strict selectable gate rejects feature skips. Root `.gitignore`/`.editorconfig` prevent generated/PDF commits and fix text format. `AGENTS.md`, `README.md`, `CONTRIBUTING.md` and the shared docs describe scope, paths, source authority, status and workflow.

`tools/check_readiness.py` + `required-tests.json` implement zero-skip presence/enabled/result checks (optional Python); `check_ownership.py` audits both rename paths and committed/staged/unstaged/untracked edits relative to the contract tag; `check_fixture_packages.py` is test-only semantic validation of tiny fixed files. None implements controller synthesis, a producer importer/exporter or model generation.

Reserved Part 2 paths: `include/symctrl/part2/README.md`, `src/part2/CMakeLists.txt`, `tests/part2/CMakeLists.txt`, `docs/part2/README.md`. Owner: independent Part 2. Target: `symctrl_part2_workspace` INTERFACE, behind SYMCTRL_BUILD_PART2 (OFF). Inputs later: a bundle plus separate task; outputs later: ISymbolicPolicy and optional internal results. Current status: reserved; no algorithms. Future sources/tests belong only in those trees. Public handoff avoids private producer classes, graph-storage assumptions and a circular producer/consumer link.

```text
Future A: GridSpec/input records -> immutable grid + catalogue
Future B: dynamics + reachability + A views + W -> immutable graph
Combined Part 1: retained grid/catalogue/graph + metadata -> ModelBundle or package
Future Part 2: bundle/package + separate specification -> ISymbolicPolicy
Future A adapter: measured state + previous memory + supplied policy -> physical input
```

Dependency direction is public contracts/common -> separate producer or consumer implementations. Fixture support depends only on common/contracts. A tests never need B. B tests use fake A views. The real shared integration test is the only feature test that links both producers. Completing A/B changes owned implementation/test/docs files, not shared client APIs, common integration source or the frozen root graph.
