# Repository instructions

This is the foundation-only baseline for the symbolic-control project. Initial bootstrap may create contracts, representation/validation helpers, fixtures, throwing stubs, tests and documentation on `main`, then commit/tag and create the two refs. It must stop there. No production A/B algorithm or Part 2 synthesis is authorised by bootstrap.

## Scope and authority

Read `docs/source-map.md` and the supplied PDFs before mathematical changes. Paper S7467 Section 1, through Section 1.2 (printed page 6 / PDF page 7), defines Part 1. Slides 14–16 define the fixed ring-building model and competition; synthesis is a later milestone. The earlier guide/reference Python code is secondary. No ODE integration or robot-angle dependency belongs in the baseline.

## Work stream and ownership

Run `git branch --show-current` and `git status --short` first. `part-a/discretization` edits only `include/symctrl/part_a/**`, `src/part_a/**`, `tests/part_a/**`, `docs/part_a/**`. `part-b/abstraction` edits only the four matching `part_b` trees. A later `part2/*` branch edits only the four matching `part2` trees. `main` contains coordinated shared foundations; never do feature implementation there. Preserve unrelated changes. Do not create another repository, discard work, rewrite history or hide conflicts.

All other tracked paths are frozen, including contracts, common code, fixtures, CLI, integration tests, root build and docs, schemas and tools. See `docs/ownership.md`. Owners extend their local CMakeLists; do not modify shared root files to add sources/tests. Shared headers include no private A/B headers. A and B depend on contracts/common, never on one another. Test-only fixture support is never linked by production targets.

If a public contract is insufficient, describe the concrete problem and proposed signature/semantic change. A coordinator makes and validates one common versioned baseline update, then both owners incorporate it before dependent work. Do not change shared tests to fit faulty implementations or invent incompatible adapters. The comparison tag for an approved update must be documented explicitly.

## Build and verification

Requires C++17, CMake >=3.21 and a C++ compiler; core build/CTest are offline and need no Python.

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

Python scripts are optional developer gates, not runtime/build dependencies. Readiness must reject missing, disabled, failed and skipped required tests. Only dedicated `symctrl::NotImplemented` permits feature skip code 77. The frozen required-test registry is a minimum; newly added tests with a selected label must also pass. Bootstrap has 9 passing foundation/handoff tests and 14 expected feature skips. Do not implement features to eliminate bootstrap skips. A/B/integration readiness intentionally fails at baseline.

## Mathematical and numerical conventions

Namespace `symctrl`; dimensions are runtime values. Internal IDs `1..M`, OUT `0`, actions `0..A-1`. Dimension 0 varies fastest. Point ownership is half-open except the last interval's outer endpoint; closed cell geometry is used for enclosures/intersections. OUT has no finite cell box and has universal successors. An enclosure touching the outer domain boundary includes OUT under the closure convention. Reject nonfinite coordinates, invalid shapes/IDs and overflow before allocation. Empty disturbance boxes/vectors are permitted only for dimension-zero disturbances. `AllStates` stores no IDs but means `0..M`; an empty Explicit set is incomplete/invalid producer output, never a safe action. All selected actions are enabled everywhere for supplied models.

Enclosures cover all starting states in a cell and disturbances in W. Cooperative monotonicity needs a documented analytic justification. Derivative bounds are nonnegative absolute-Jacobian bounds over the whole requested box, with row-major shapes n×n and n×q. No centre-only fallback or signed radius. Ordinary double arithmetic is `UncertifiedFloat`, even when tests pass. See `docs/contracts.md` for exceptions, ownership and exact signatures.

Keep source PDFs local and ignored; generated trees, exports and benchmarks are ignored. Do not add synthesis or run large benchmarks during bootstrap. Do not push unless separately authorised by the user. No extra agents during initialisation.
