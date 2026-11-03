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

All production functions remain stubs/reserved. Legacy named-example files in the v1 tree do not define B's general responsibilities. Their separation/delegation is a coordinated future code change; this task edits documentation only.

## Independent verification

Use test-only dynamics with different state/input/disturbance dimensions, positive and mixed-sign coefficients and q=0. Use fake grid/input interfaces rather than waiting for A. Check full-box coverage, malformed derivative bounds, unsupported capabilities, closed-boundary intersections, OUT, sorted unique nonempty successors, graph lifetime and processing/storage limits.

Do not replace reachable enclosures with centre simulation. Derivative matrices bound absolute values over the whole requested box. Unsupported methods fail explicitly. Graphs must own their output after build arguments disappear. Export every state/action pair, including universal OUT rows. Doubles remain UncertifiedFloat.

Part 2 consumes the resulting model to implement general synthesis; B does not hard-code a specification or a physical example into the graph builder. A later mobile robot must use the same interfaces; its periodic heading requires a sound model/enclosure adapter or a coordinated extension, never just wrapping a centre.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_b --output-on-failure
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_ownership.py auto
```

The eight existing B cases belong to the original scaffold and currently skip on missing implementations. Their legacy example coverage is not a full generic-controller acceptance suite. Add meaningful generic cases through B's local test CMake file, with label part_b, during authorised implementation. Preserve old shared assertions and coordinate registry/API changes.

The existing B worktree is /home/ilyazz/symctrl-b. This documentation task does not edit that checkout or change refs. Bring a coordinated shared update into the owner branch using its documented comparison baseline before relying on new contracts.
