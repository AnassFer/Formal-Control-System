# General Part 1 → Part 2 handoff, contract/schema 1.0.0

Part 1 produces a generic transition model; Part 2 uses it with a separate specification to synthesise a policy. Keep both stages model-independent. A robot is a later demonstration, not a special branch inside general solvers. Current scope is in [scope-correction.md](scope-correction.md).

The existing model-exchange contract remains version 1.0.0 at the original part1-contract-v1 baseline. This documentation revision changes neither public C++ declarations nor serialization. Planned synthesis/specification APIs are not implemented merely because a model/policy boundary exists.

## Immutable C++ route

ModelBundle owns shared_ptr<const IStateGrid>, shared_ptr<const IInputCatalog>, shared_ptr<const ITransitionSystem> and matching owned ModelMetadata. Metadata records versions, identifier/parameters, source/provenance, n/m/q, M/A, OUT, numeric status, grid and disturbance bounds. validateBundle checks trivial handle/metadata consistency, not every graph edge or numerical soundness. Treat published views/metadata as immutable.

Fixed bundles are available from fixtures/support/fixture.hpp; use fixtures::nondeterministicChoice() to study the generic graph/specification example. The working consumer is examples/part2_fixture_consumer.cpp. These fixtures use literal records rather than A/B factories.

The future producer's existing assembly boundary is assembleModelBundle(shared grid,shared inputs,shared transitions,metadata). It remains a B-owned stub. Graph construction through buildSymbolicModel consumes generic grid/input/enclosure views and owns its output independently of the build arguments. Input/successor references remain valid for their immutable view lifetime; cellBounds returns an owned closed Box.

Query states are 0..M and actions 0..A-1. OUT is state 0; geometry exists only for 1..M. All actions are enabled in all states under the current complete-model contract.

Use contains(set,id,M), nonempty(set,M) and forEachSuccessor(set,M,visitor). AllStates has empty storage but means every ID 0..M; membership/OUT detection needs no universal allocation. Explicit IDs are sorted unique values in range. Empty Explicit is representable diagnostically but invalid/incomplete producer output here. Check nonemptiness before any universal control test. Iteration is ascending and preserves the representation.

## Meaning for general synthesis

Successors represent nondeterminism. Robust control must meet the objective for every successor. One favourable edge/path or a probability interpretation is insufficient. OUT is normally excluded from safe sets, and AllStates therefore includes losing behaviour. Empty successors cannot create successful actions through vacuous truth. Disabled inputs need a versioned extension rather than a missing-row convention.

Applications/examples supply safe/target sets, labels and optional specification automata separately from the model. Use whole-cell inclusion when conservatively labelling a region as safe; overlap or centre inclusion alone is insufficient. Cell/successor geometry uses closures; point ownership is half-open except the final upper boundary. Outer-boundary contact can include OUT even if that exact point belongs internally.

Part 2 may return an ISymbolicPolicy. Its decide(previous_memory,current_state) returns next_memory and action, updating memory before action choice. Initial memory defaults to 0 unless explicitly specified otherwise. Preserve the model's action catalogue mapping. Missing entries/invalid memory fail explicitly. A's adapter rejects OUT/invalid actions and returns an owned physical vector without a fallback. No policy-synthesis factory is implemented now; its precise public API requires a common update.

Fixtures are hand-authored and ordinary floating arithmetic is uncertified. Neither fixture compatibility nor successful synthesis upgrades numerical status to a certified physical guarantee.

## Language-independent directory format

A complete v1 model package consists of:

```text
manifest.json
cells.csv
inputs.csv
transitions.csv
```

The [schema](../schemas/part1-model-v1.schema.json) is JSON Schema draft 2020-12. A complete generic example manifest is [the choice fixture](../fixtures/part1_v1/nondeterministic_choice/manifest.json).

| Required manifest field | Meaning |
|---|---|
| schema_version, contract_version | 1.0.0 strings |
| model_id, model_parameters | Nonempty identifier and finite numeric parameter map; no solver special case |
| provenance, source | hand_authored_fixture or generated_part1 plus nonempty source description |
| dimensions | Positive state/input and nonnegative disturbance dimensions |
| internal_states, input_count, outside_state_id | Positive M/A and OUT=0 |
| grid | Finite positive-width domain and positive uint32 per-axis cell counts with product M |
| semantics | Fixed ordering, ownership, closure, OUT and enabled-action meanings below |
| numeric_guarantee | uncertified_float or genuinely justified validated_enclosure |
| disturbance | q-dimensional lo/hi arrays; both empty for q=0 |
| files | cells.csv, inputs.csv, transitions.csv as relative fixed filenames |

The seven semantics fields are state_order=dimension_0_fastest, input_order=explicit_caller_order or cartesian_dimension_0_fastest, point_ownership=half_open_last_closed, cell_geometry=closed, successor_intersection=closure, outside_successors=all_states and enabled_inputs=all_states_all_inputs.

CSV headers expand dimensions literally:

```text
state_id,lower_0,...,lower_(n-1),upper_0,...,upper_(n-1)
input_id,u_0,...,u_(m-1)
state_id,input_id,kind,successor_id
```

Ellipses describe expansion, not characters in actual files. All files use UTF-8 without BOM; CSV uses comma, LF/final newline, decimal point and no comment/blank rows. Headers are literal. IDs use unsigned decimal without leading zeros except 0 and fit uint64; M<=UINT64_MAX-1. Coordinates are finite decimal tokens with optional fraction/exponent, no NaN/infinity/hex/locale comma. Future double export uses max_digits10 round-trip precision, normally 17 significant digits; exact integer-looking coordinates can be written as integers.

Cells have exactly M rows ordered 1..M, correct n-dimensional closed grid boxes and no finite OUT row. Inputs have exactly A rows ordered 0..A-1, correct physical-vector dimensions and caller/Cartesian ordering; duplicate physical vectors may have distinct IDs.

Transitions sort numerically by state/action/successor and cover every pair including OUT. Explicit rows have one nonempty successor ID each, grouped sorted/unique by pair. An all pair has exactly one row with an empty fourth field/trailing comma and means every 0..M. Never mix all and explicit rows for one pair. Every OUT/action pair is universal. Reject missing pairs, duplicate rows, invalid IDs/kinds/shapes, inconsistent counts/geometry and empty Explicit producer results. There is no implicit disabled/missing-pair convention.

The schema checks structure; cross-file dimensions/products/completeness and numerical truth require semantic validation. tools/check_fixture_packages.py is an optional checker for fixed samples, not a production importer/exporter. Public production export remains a stub.

Consumers reject unsupported major versions. Current helpers/schema accept exactly 1.0.0. Negotiate compatible additions; incompatible ID/boundary/transition/signature meaning requires coordinated versioning for both owners and consumers.

## Fixture and client base

The generic choice fixture has states 1,2,3, OUT 0, two actions, 1D domain [0,3] and unit cells. At state 1, action 0 reaches {2,3}, action 1 reaches {2}; state 2 self-loops at 2, state 3 at 3, and OUT is universal. The separate specification has safe={1,2}, target={2} and a manually checked expected action 1 at state 1. This is fixed data, not computed synthesis.

symctrl_fixture_support, part2_fixture_consumer and part2_handoff_tests link only common/contracts/fixtures, never A/B production libraries. [The quickstart](part2-quickstart.md) covers same/separate repository arrangements. Later replace fixed data with real generic producer output and add algorithm-level integration checks; do not claim a fixture pass proves the completed pipeline.
