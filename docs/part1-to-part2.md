# Part 1 → Part 2 handoff, contract/schema 1.0.0

Baseline reference: local tag `part1-contract-v1`. This handoff works now using **hand-authored fixtures**, without implementing any Part 1 factory. It is not production output or a certification claim. Complete public declarations are in `include/symctrl/contracts/`; [contracts.md](contracts.md) specifies all C++ signatures, shapes, exceptions and lifetimes.

## In-process route

`ModelBundle` owns three `std::shared_ptr<const ...>` handles: IStateGrid for geometry/abstraction, IInputCatalog for physical actions, and ITransitionSystem for successors. Owned ModelMetadata includes versions, model identifier/parameters, provenance, source, n/m/q, M/A, OUT, numerical status, grid and disturbance bounds. `validateBundle(bundle)` checks that handles and metadata agree. It does not recompute transitions or certify the model. A published bundle is treated as immutable; copies retain safe handle ownership.

Use `fixtures::thermostat()` or `fixtures::nondeterministicChoice()` from `fixtures/support/fixture.hpp` now. These functions look up tiny literal tables. Later the producer will use the exact B factory `assembleModelBundle(shared_ptr<const IStateGrid>, shared_ptr<const IInputCatalog>, shared_ptr<const ITransitionSystem>, ModelMetadata)->ModelBundle`, after constructing a real graph through `buildSymbolicModel(...)`. That production assembly symbol is currently a throwing stub; fixtures do not call it.

Queries accept state IDs 0..M and actions 0..A-1. Geometry exists only for 1..M. `cellBounds` returns an owned closed Box; input/successor const references live with their immutable view objects and remain valid across queries. A graph owns its data independently of grid/input/enclosure build arguments. Consumers depend on public contracts, never A/B private headers or storage layout.

Use `contains(set,id,M)`, `nonempty(set,M)` and `forEachSuccessor(set,M,visitor)` from transitions.hpp. AllStates stores an empty vector yet means **every ID 0..M**; membership and OUT detection do not allocate. Iteration visits ascending IDs, including OUT. Explicit IDs must be sorted, unique and in range. An empty Explicit set is mathematically empty, but an invalid/incomplete result for these complete models. Check nonemptiness before a robust universal condition. All selected actions are enabled for every state; disabled actions need a versioned extension.

## Semantics and task separation

Successors are nondeterministic. Robust control requires an action to meet the objective for **every** successor. One favourable edge, one BFS path or probability interpretation does not establish correctness. OUT is normally excluded from safe sets, so a universal successor includes losing behaviour. An empty Explicit set cannot create a successful action through vacuous truth.

Point abstraction is half-open with the last outer endpoint included; cell geometry and successor tests use closures. Outer-boundary contact can include OUT even when that point belongs to an internal cell. Region safety normally requires a whole closed cell to be included in a safe region; overlap or centre inclusion alone is insufficient. Labels, safe/target sets and any specification automaton come from the separate Part 2 specification, not from Part 1.

Part 2 may return an `ISymbolicPolicy` with `PolicyDecision decide(PolicyMemory previous_memory,StateId current_state) const`. Supply psi(-1), default `PolicyMemory{0}`, then feed each returned next_memory into the following call. The decision updates memory before action selection. Action IDs keep exactly the bundle's catalogue mapping. Missing entries/invalid memory raise out_of_range; the Part A adapter rejects OUT and invalid actions without a fallback. Richer synthesis internals stay private to Part 2. No policy generator is provided now.

NumericalGuarantee is truthful: current fixtures and future ordinary-double code are `uncertified_float`. A mathematically sound formula plus passing tests does not establish validated numerical enclosures. A future `validated_enclosure` producer must supply an actual rounding/error-enclosure justification in provenance/source documentation. Fixture compatibility cannot prove future physical or synthesis correctness.

## Language-independent directory package

Exactly four model files constitute a complete v1 package:

```text
manifest.json
cells.csv
inputs.csv
transitions.csv
```

The complete JSON Schema is [../schemas/part1-model-v1.schema.json](../schemas/part1-model-v1.schema.json), draft 2020-12. The complete concrete manifest is [../fixtures/part1_v1/thermostat/manifest.json](../fixtures/part1_v1/thermostat/manifest.json); the second package has its own complete manifest. No field is undecided.

| Required manifest field | Frozen meaning |
|---|---|
| schema_version, contract_version | Strings `1.0.0` |
| model_id | Nonempty identifier; agrees with separate specification if supplied |
| model_parameters | Object mapping parameter names to finite numbers; e.g. a=.1,b=.2,T=30 |
| provenance | `hand_authored_fixture` or future `generated_part1` |
| source | Nonempty human-readable provenance; include numerical justification if validated |
| dimensions | Object: positive integer state/input dimensions, nonnegative disturbance dimension |
| internal_states, input_count | Positive M excluding OUT and positive A |
| outside_state_id | Integer 0 |
| grid | Object with domain `{lo,hi}` arrays of length n and cells_per_dimension of n positive uint32 counts, product M |
| semantics | All seven fixed fields listed below |
| numeric_guarantee | `uncertified_float` or justified `validated_enclosure` |
| disturbance | `{lo,hi}` arrays of length q; both empty when q=0 |
| files | Relative filenames: cells=`cells.csv`, inputs=`inputs.csv`, transitions=`transitions.csv` |

Semantics fields are `state_order=dimension_0_fastest`, `input_order=explicit_caller_order` or `cartesian_dimension_0_fastest`, `point_ownership=half_open_last_closed`, `cell_geometry=closed`, `successor_intersection=closure`, `outside_successors=all_states`, `enabled_inputs=all_states_all_inputs`. Even when a producer uses Cartesian inputs, inputs.csv records every selected physical vector in its final action-ID order; consumers need no enumeration algorithm.

JSON and CSV use UTF-8 without BOM. CSV uses LF newlines, a final newline, ASCII comma separators and a decimal point independent of locale. There are no comment/blank rows or omitted columns. Headers are literal and have no quotes/spaces. Canonical integer fields use decimal digits, no sign/leading zeros except `0`, and must fit uint64; M<=UINT64_MAX-1. Numeric vector fields are finite JSON-style decimal tokens, optional decimal fraction and exponent, no NaN/infinity/hex/locale comma. Future double export uses sufficient round-trip precision (`std::numeric_limits<double>::max_digits10`, normally 17 significant digits); exact integer-looking coordinates may be written `17`. These tiny fixed fixture decimals are exactly the specified source literals, not the output of an exporter.

```text
cells.csv:
state_id,lower_0,...,lower_(n-1),upper_0,...,upper_(n-1)
inputs.csv:
input_id,u_0,...,u_(m-1)
transitions.csv:
state_id,input_id,kind,successor_id
```

The ellipses describe header expansion only; concrete files have expanded headers such as `state_id,lower_0,upper_0`. Cells have exactly M rows in ID order 1..M; no finite OUT box exists. Closed bounds agree with the uniform grid/domain and dimension-0-fastest IDs. Inputs have exactly A rows in ID order 0..A-1; duplicate physical vectors are allowed as distinct actions, duplicate IDs are not.

Transition rows sort lexicographically by numeric state_id, input_id and successor_id. They cover **every** state/action pair, including OUT. `kind=explicit` has one nonempty integer successor per row; grouped rows for a pair form its sorted unique set. `kind=all` has exactly one row and an empty fourth field (including the trailing comma), meaning all 0..M. Never combine all and explicit for one pair. OUT has one all row per valid action. Missing pairs, duplicate rows, invalid IDs, malformed fields, inconsistent counts/dimensions/geometry or empty explicit results are rejected. There is no missing-row convention for disabled actions or empty sets. Universal rows cannot be mistaken for emptiness.

The JSON Schema checks manifest structure and fixed meanings; dynamic shape/product/cross-file completeness and numerical/provenance truth require semantic validation. `tools/check_fixture_packages.py` is an optional test-only semantic checker for these tiny packages, not a production importer/loader/exporter. C++ handoff tests compare complete literal CSV tables and public views. A consumer's future loader must reject inconsistent packages before exposing a model.

v1 readers reject unsupported major versions. This baseline's C++ helper and schema accept exactly 1.0.0; compatible additions must be explicitly negotiated. No “minor” addition can silently change IDs, transitions, enabled actions or boundary meaning. Incompatible C++ signatures or serialization require coordinated versioning with A, B and the Part 2 group.

## Fixtures available immediately

* `fixtures/part1_v1/thermostat/`: manifest, three closed boxes, two inputs, the six oracle successor sets, both universal OUT rows, plus `expected-output.txt`. No algorithm generated them.
* `fixtures/part1_v1/nondeterministic_choice/`: three regular states, 1D domain [0,3], unit cells and physical inputs [0]/[1]. Edges: (1,0)→{2,3}, (1,1)→{2}, (2,0/1)→{2}, (3,0/1)→{3}, OUT→AllStates. `specification.json` separately supplies safe={1,2}, target={2}, initial memory 0 and a **manually checked** expectation: state 1 should use action 1, because action 0 can reach unsafe state 3. This statement is not a computed synthesis result.

`examples/part2_fixture_consumer.cpp` prints fixture metadata, successor sets including OUT, cell bounds, physical actions and membership/iteration. It performs no search, synthesis, dynamics or model generation. It links only `symctrl_fixture_support -> symctrl_common -> symctrl_contracts`, as do `part2_handoff_tests`. [part2-quickstart.md](part2-quickstart.md) supplies tested same/separate repository commands.
