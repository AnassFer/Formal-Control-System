# Part B: general reachability and transition-model engine

Branch part-b/abstraction. Edit only include/symctrl/part_b/, src/part_b/, tests/part_b/ and docs/part_b/. B and A together implement the paper's general first part. Read [the scope note](../scope-correction.md).

## What B builds

B receives an IDynamics implementation, grid/input views, disturbance bounds and options. It supplies conservative enclosures, a finite nondeterministic transition model, immutable bundles, query/storage/export support and truthful numerical metadata. It must not inspect a model name, embed physical constants or assume a fixed coordinate count.

| Order | Existing source under src/part_b/ | General responsibility |
|---|---|---|
| 1 | monotone_reachability.cpp | Endpoint enclosure for a model with justified cooperative monotonicity |
| 2 | derivative_reachability.cpp | Full-box absolute-Jacobian enclosure, including mixed-sign models |
| 3 | factories.cpp | makeReachability dispatch using an injected dynamics handle and options |
| 4 | explicit_transition_system.cpp | Owned sparse successor storage and compact AllStates representation |
| 5 | model_builder.cpp | Grid/input/enclosure-driven transitions, OUT, budgets and bundle assembly |
| 6 | model_export.cpp | Complete generic model-package export through the frozen format |

All B production factories and the six implementation milestones are complete: both reachability methods, immutable transition storage/construction, bundle assembly, complete package export and the existing thermostat/building adapters. The generic engine accepts supplied dynamics; named-example files do not define its algorithms. Public contracts, schema, shared tooling and A implementation files are unchanged.

Detailed notes: [derivative backend](derivative.md), [graph/bundles](graph.md), [model adapters](dynamics.md), [export](export.md) and [validation results](validation.md). Shared root status documents describe the earlier common scaffold; this owner-local guide records current B progress pending coordinator integration.

## Monotone reachability

For a valid closed state cell C, fixed physical input u and disturbance box W, makeReachability with CooperativeMonotone retains the supplied IDynamics and returns a backend with matching runtime dimensions. State/input dimensions must be positive; disturbance dimension may be zero. The private backend construction declaration is in include/symctrl/part_b/reachability_backends.hpp; public clients continue to use contracts/factories.hpp.

Every enclose request validates finite coordinates, shapes and bound ordering before calling the model. It asks cooperativeOn(C,u,W) for an analytic justification over the complete request, then returns [f(C.lo,u,W.lo), f(C.hi,u,W.hi)]. False capability results raise UnsupportedCapability. Capability is checked anew for each box/input, and no derivative capability is needed. For q=0, W has empty lower/upper vectors and both step calls receive the empty disturbance vector. Degenerate closed boxes are supported.

Both endpoint outputs must be finite state-dimensional vectors with lower <= upper componentwise. Malformed callback outputs, including nonfinite coordinates, raise invalid_argument; endpoints are never silently reordered. Exceptions raised by the model itself propagate, including overflow_error. Ordinary endpoint arithmetic reports UncertifiedFloat, and require_validated=true raises UnsupportedCapability. Null dynamics, zero state/input dimensions and unknown reachability methods raise invalid_argument. BoundedDerivative selects the implemented [derivative backend](derivative.md).

Nine focused cases in tests/part_b/monotone_reachability.cpp cover analytic affine bounds, different state/input/disturbance dimensions, q=0, point boxes, box/input-dependent capabilities, malformed requests/outputs, factory validation, retained ownership and model exceptions. Their executable links only B/common/contracts and shared test support, with no A or fixture dependency. These completed cases fail rather than skip if NotImplemented is thrown.

From the B checkout, with CMake >=3.21 and a C++17 compiler available:

```sh
cmake --preset dev
cmake --build build/dev --target monotone_reachability_tests --parallel 2
ctest --test-dir build/dev -R '^part_b\.monotone\.' --output-on-failure --no-tests=error
python3 tools/check_ownership.py auto --base part1-docs-v2
```

The explicit comparison tag excludes the coordinator's documentation update from B's ownership audit; the original part1-contract-v1 tag stays fixed. Existing shared contracts/tests remain unchanged.

Current verification: the strict B gate passes 56 tests with zero skips, including all 47 B cases and nine contract/handoff checks. GCC Debug and Clang Release with address/undefined/leak sanitizers pass. Ownership against part1-docs-v2 and git diff --check pass. A temporary merged snapshot with the existing A branch passes all 62 integration-gate tests without changing either branch. Exact revisions, commands and limits are recorded in [validation.md](validation.md).

## Independent verification

Use test-only dynamics with different state/input/disturbance dimensions, positive and mixed-sign coefficients and q=0. Use fake grid/input interfaces rather than waiting for A. Check full-box coverage, malformed derivative bounds, unsupported capabilities, closed-boundary intersections, OUT, sorted unique nonempty successors, graph lifetime and processing/storage limits.

Do not replace reachable enclosures with centre simulation. Derivative matrices bound absolute values over the whole requested box. Unsupported methods fail explicitly. Graphs must own their output after build arguments disappear. Export every state/action pair, including universal OUT rows. Doubles remain UncertifiedFloat.

Part 2 consumes the resulting model to implement general synthesis; B does not hard-code a specification or a physical example into the graph builder. A later mobile robot must use the same interfaces; its periodic heading requires a sound model/enclosure adapter or a coordinated extension, never just wrapping a centre.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2 --target \
  part_b_tests dynamics_tests monotone_reachability_tests \
  derivative_reachability_tests model_builder_tests model_export_tests \
  contract_tests part2_handoff_tests part2_fixture_consumer
ctest --test-dir build/dev -L 'contracts|handoff|part_b' --output-on-failure --no-tests=error
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_ownership.py auto --base part1-docs-v2
```

These selected build targets do not build/link A. All eight original B cases and 39 focused cases pass; completed focused tests reject missing implementations rather than skipping. A full default build on this B branch has 56 passes and six expected skips because A factories are still stubs here (five A cases and one integration case). The separate merged-snapshot check substitutes the real A branch through a normal Git tree merge.

Part 2 synthesis and the later robot example remain separate work. B readiness establishes the implemented Part 1 model boundary, not numerical certification or a full controller. No benchmark, shared API/schema update, merge into main or push is part of this B implementation.

The existing B worktree is /home/ilyazz/symctrl-b. Feature work stays on part-b/abstraction and inside B's four trees. Bring any coordinated shared contract update into the owner branch using its documented comparison baseline before relying on new contracts.
