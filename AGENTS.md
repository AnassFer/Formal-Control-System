# Repository instructions

## Scope and authority

Follow the latest user instruction and `docs/scope-correction.md`: keep the paper implementation general, prepare paper Part 1 and a base for Part 2, and add a mobile-robot example only after the reusable pipeline is ready. Development A/B are ownership tracks within Part 1. Do not infer different course ownership from an article section number.

Paper Section 1 defines general abstraction/refinement; Section 2 defines general synthesis; Section 3 supplies a later example. Read `docs/source-map.md` and supplied documents before mathematical changes. The original bootstrap prompt is historical; later explicit user directions take precedence. The current request is **documentation only**. Do not implement algorithms or change production C++, contracts, schemas, CMake, tests or readiness tooling in this revision.

## Work streams and ownership

Run `git branch --show-current` and `git status --short` first. Preserve unrelated changes. Main holds coordinated docs/contracts/build updates; feature implementation belongs on its owner branch.

* `part-a/discretization`: only include/symctrl/part_a/**, src/part_a/**, tests/part_a/**, docs/part_a/**. Owns general grids, inputs and applying a supplied policy with memory. Responsibilities/signatures unchanged.
* `part-b/abstraction`: only the four corresponding part_b trees. Owns general dynamics/enclosure integration, transition construction/storage/query/export and immutable model bundles. Model implementations are supplied through interfaces; examples do not define the engine.
* A later `part2/*` stream: only the four corresponding part2 trees. Provides general symbolic-controller synthesis on the shared model/specification/policy boundary. It is currently a reserved base, not an implementation.

Shared paths include contracts/common, root build/docs, fixtures, schemas, CLI, tools and common tests. The user authorises the current coordinator Markdown correction. Feature owners add sources/tests through local CMake files rather than root edits. Shared headers include no private owner headers. Production targets do not link fixture implementations. Keep A and B independently testable through public views.

If a public contract is insufficient, describe the concrete failing use case, exact proposed change/version impact and affected tests. Validate one common update for all consumers. Do not independently alter shared interfaces, weaken tests or hide incompatible behaviour with adapters. Current v1 signatures/schema and historical tags remain unchanged until a coordinated code update.

## Build and verification

Requires C++17, CMake >=3.21 and a compiler; ordinary C++ build/CTest needs no network or Python.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure --no-tests=error
ctest --test-dir build/dev -L 'contracts|handoff' --output-on-failure
ctest --test-dir build/dev -L part_a --output-on-failure
ctest --test-dir build/dev -L part_b --output-on-failure
ctest --test-dir build/dev -L integration --output-on-failure
python3 tools/check_readiness.py build/dev --mode foundation
python3 tools/check_readiness.py build/dev --mode a
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_readiness.py build/dev --mode integration
python3 tools/check_ownership.py auto
```

These existing checks cover the original scaffold and fixtures, not a complete general controller. The original foundation had 9 passes and 14 feature skips. Only NotImplemented permits skip code 77. Required readiness rejects missing, disabled, failed or skipped tests and zero-test runs. Generic synthesis and later example acceptance coverage remain to be added. Python gates are optional developer tools.

## Mathematical and numerical conventions

Namespace symctrl; runtime dimensions; internal IDs 1..M, OUT 0, actions 0..A-1. Dimension 0 varies fastest. Point ownership is half-open except the last upper endpoint. Geometry/intersections use closed boxes. OUT has no finite bounds and universal successors. Outer-boundary enclosure contact includes OUT under the current closure convention. Reject nonfinite values, invalid dimensions/IDs and overflow before allocation. Empty boxes/vectors describe only zero-dimensional disturbances.

AllStates has empty storage but means every ID 0..M. Empty Explicit results cannot create vacuous successful control choices. Current complete models enable all selected actions in all states. Later robust synthesis must account for every successor and receive its specification separately.

Enclosures cover every cell point and disturbance. Monotonicity needs analytic justification; derivative bounds are nonnegative absolute bounds over the whole box, shaped n×n and n×q. Unsupported capabilities fail explicitly. A later periodic-heading example needs sound interval/topology handling; wrapping only a centre is insufficient. Ordinary doubles remain UncertifiedFloat; tests do not certify physical guarantees.

Keep source PDFs and the original prompt as local historical inputs, generated output out of Git and unrelated work intact. No nested repository, destructive Git operation, history rewrite or silent tag move. Do not push without user authorisation. Do not spawn extra agents unless explicitly authorised. No large benchmarks during documentation work.
