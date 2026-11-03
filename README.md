# General symbolic-control template

Read [START_HERE.md](START_HERE.md) for folders, branches and each person's work. The [scope note and repository audit](docs/scope-correction.md) explain the current model-independent plan.

| Stage | Deliverable |
|---|---|
| Paper Part 1 | General abstraction/refinement foundation, implemented in A/B tracks |
| Paper Part 2 | General symbolic-controller synthesis on that foundation; reserved base supplied now |
| Later example | Demonstrate the completed reusable pipeline with a mobile robot |

A owns grids, input catalogues and policy refinement. B owns general reachability, transitions, storage/export and model bundles. The Part 2 base exposes the model and policy boundary for later synthesis. Keep all these stages general; no particular physical example defines the solver.

## Pipeline

```text
Supplied dynamics + domains + inputs + disturbances -> transition model
Transition model + separate specification -> symbolic policy
Measured state + policy + previous memory -> physical input
```

A discretises a continuous-state domain. B encloses every possible next state from each closed cell under a chosen input and allowed disturbances. The resulting finite graph is nondeterministic. Later synthesis must satisfy its objective for every possible successor. A's runtime adapter applies the supplied policy and returns an owned physical input.

## Current status

**This is a template, not a completed controller.** Public interfaces, small common helpers and fixed fixture consumers work. Production A/B factories remain unimplemented. Part 2 algorithms and the robot example are not implemented. This correction changes Markdown only.

The core IDynamics, IReachability, ITransitionSystem, ModelBundle and policy interfaces are model-independent. The original v1 tree also contains legacy example-specific scaffolding; it is outside the required generic design and unchanged by this documentation task. Separate such adapters from core algorithms in a coordinated later code update.

## Build existing checks

C++17, CMake >=3.21 and Make are required; GCC/Clang on Linux are supported. Ordinary C++ builds/tests need no network or Python.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure --no-tests=error
```

Original scaffold result: **9 passes, 14 expected feature skips, 0 failures**. Only NotImplemented permits a skip. These checks do not prove full-paper functionality; generic synthesis and later robot acceptance tests do not exist yet. See the [historical validation record](docs/bootstrap-validation.md).

Optional developer gates:

```sh
python3 tools/check_fixture_packages.py
python3 tools/check_readiness.py build/dev --mode foundation
python3 tools/check_readiness.py build/dev --mode a
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_readiness.py build/dev --mode integration
```

Foundation readiness passes when its nine checks are built; feature readiness currently rejects missing implementations. The registry must evolve with agreed generic acceptance tests before advertising completion. Release, sanitize and SDK presets are available.

## Ownership

| Owner | Feature paths | Main responsibility |
|---|---|---|
| A | `{include/symctrl,src,tests,docs}/part_a/` | Grids/inputs and runtime refinement |
| B | `{include/symctrl,src,tests,docs}/part_b/` | General reachability/transition-model production |
| Part 2 | `{include/symctrl,src,tests,docs}/part2/` | Later general controller synthesis |
| Coordinator | Shared contracts/build/docs/fixtures/tests | Common versions and integration |

Brace notation abbreviates four real directories; [ownership.md](docs/ownership.md) lists them. Each owner extends local CMake files. Contracts include no private A/B header. The original part1-contract-v1 tag stays fixed; this task changes no branch ref or separate worktree.

Read [A's notes](docs/part_a/README.md), [B's notes](docs/part_b/README.md), [the blueprint](docs/part1-final-structure.md), [status](docs/implementation-status.md), [contracts](docs/contracts.md) and [Git workflow](docs/git-workflow.md).

## Part 2 starting base

Read [PART2_HANDOFF.md](PART2_HANDOFF.md), [the quickstart](docs/part2-quickstart.md) and [the API/file-format handoff](docs/part1-to-part2.md).

```sh
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
```

The fixture consumer and nine contract/handoff checks work without A/B libraries. Fixtures are fixed examples, not production algorithm output. The earlier generated handoff archive has historical documentation; use the current Markdown as guidance.

No algorithms, public APIs, schema, CMake or tests are changed by this documentation-only revision. No commit or push is performed. Ordinary doubles remain uncertified.
