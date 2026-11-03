# Start here: the general symbolic-control template

This guide explains the repository and where each person works. The project stays general through the paper's first and second parts. A mobile robot will be used as an example after the reusable pipeline is ready.

## 1. What the project does

A real state can have continuously many values. We divide a bounded state domain into numbered cells and select a finite catalogue of input vectors. For each cell and selected input, we enclose every possible next physical state, accounting for the whole cell and all allowed disturbances. The cells that the enclosure intersects become possible symbolic successors.

Later, a synthesis algorithm receives this finite graph and a separate objective, then chooses actions that meet the objective for every possible successor. At runtime, an adapter maps the measured physical state to a cell, asks the policy for an action and returns its physical input vector.

| Stage/person | Job |
|---|---|
| Part 1, person A | General grids, input catalogues and runtime refinement of a supplied policy |
| Part 1, person B | General reachability enclosures, transition graph, bundles and export |
| Part 2 team/base | Later general controller synthesis from a model and a separate specification |
| Later example | Use the completed engine with a mobile robot |

A/B are development owners inside Part 1, not different physical applications. [The scope note](docs/scope-correction.md) and [source map](docs/source-map.md) explain the paper-stage boundaries.

## 2. What is already implemented?

The repository is a foundation. Shared types/interfaces, small validation/representation helpers and fixed fixture consumers work. Production A/B factories are stubs that throw NotImplemented. Part 2 synthesis and the robot example do not exist yet.

A fixture is fixed example data, written by hand so another person can test a consumer before a producer is complete. A stub is a function with the agreed signature but no production implementation. Neither should be described as a finished engine.

The original common starting point is tag part1-contract-v1, commit 25c6fe909cac531c2a3e1d2593cd598ff4522064. The tag remains fixed. This documentation revision changes no C++/build/test file, branch ref or implementation status.

## 3. Useful words

| Word | Meaning |
|---|---|
| Repository/root | Project and Git history / its top folder containing CMakeLists.txt |
| Commit | A saved local snapshot |
| Branch | A named line of development such as part-b/abstraction |
| Tag | A label attached to one particular commit |
| Clone | A separate copy of a repository, often on another computer |
| Worktree | Another folder checked out on a branch of the same local repository |
| Push | Send local commits/refs to a remote server |
| Interface/contract | Agreed operations, shapes, errors and meanings other code relies on |
| Factory | A function creating an object, such as makeUniformGrid |
| Build | Compile sources into libraries/executables |
| Successor | A possible next symbolic state after an action |
| Policy | A rule returning an action and possibly updated memory |
| SDK | Public headers/helpers/examples another project can build against |

.hpp files describe types/interfaces; .cpp files contain implementations. Here public contracts are shared and owner-specific source directories hold future implementations.

## 4. Folder map

```text
Automate Project/
  START_HERE.md                  This guide
  README.md                      Overview and build commands
  AGENTS.md                      Coding-assistant instructions
  CONTRIBUTING.md                Contribution rules
  CMakeLists.txt                 Shared top-level build
  CMakePresets.json              dev/release/sdk/sanitize settings
  include/symctrl/contracts/     Shared public types/interfaces
  include/symctrl/part_a/         A's private headers
  include/symctrl/part_b/         B's private headers
  include/symctrl/part2/          Later Part 2 headers
  src/common/                    Completed shared helpers
  src/part_a/                    A implementation + local CMake
  src/part_b/                    B implementation + local CMake
  src/part2/                     Reserved synthesis base + local CMake
  tests/                        Checks separated by purpose/owner
  fixtures/                     Fixed example models and adapters
  examples/                     Existing fixture consumer/standalone client
  apps/                         Legacy CLI skeleton; no completed general CLI
  schemas/                      Model-package format
  docs/                         Contracts, plans and owner notes
  tools/                        Ownership/readiness/fixture checks
  .github/workflows/             CI setup
  Project files/                Local source PDFs; not committed
  build/                        Generated output; not committed
  .git/                         Git internal data; manage through Git
```

Some local/generated folders appear only after setup. The existing v1 source tree also contains legacy example-specific scaffolding. It does not define the general design and is unchanged by this Markdown-only task.

The contract headers describe: types/validation (types.hpp), grids (grid.hpp), input vectors (inputs.hpp), injected dynamics (dynamics.hpp), enclosures (reachability.hpp), successor sets/graphs (transitions.hpp), policy execution (policy.hpp), bundles (model_bundle.hpp) and public factories (factories.hpp).

## 5. Where each person edits

| Owner | Allowed paths |
|---|---|
| A | include/symctrl/part_a/, src/part_a/, tests/part_a/, docs/part_a/ |
| B | include/symctrl/part_b/, src/part_b/, tests/part_b/, docs/part_b/ |
| Part 2 | include/symctrl/part2/, src/part2/, tests/part2/, docs/part2/ |
| Coordinator | Shared contracts/build/docs/fixtures/common tests and integration |

Everyone can read all files. Ordinary feature changes stay in the owner's four trees. Local source/test CMakeLists are already included by the root; extend them rather than editing the root build for each new file. A necessary shared change is agreed and validated once for all consumers.

## 6. Tools and current location

You need Git, a C++17 compiler, CMake >=3.21 and Make. CTest comes with CMake. Python 3 is only optional developer tooling.

```sh
git --version
g++ --version
cmake --version
ctest --version
python3 --version
```

The original machine used a temporary CMake installation. If still present, this adds it for your terminal:

```sh
export PATH="/tmp/symctrl-cmake/cmake/data/bin:$PATH"
```

That path is machine-specific and temporary. Other computers need their own CMake installation. Run commands from the relevant checkout root; use pwd and git branch --show-current to check where you are. Quote paths containing spaces.

## 7. Branches on this computer and another computer

A/B branches already exist locally. Do not run git init again or recreate someone's branch. The original checkout remains on main for coordinated work; each active owner should have a separate folder.

On this computer, the existing B worktree is /home/ilyazz/symctrl-b:

```sh
cd "/home/ilyazz/Automate Project"
git worktree list
cd /home/ilyazz/symctrl-b
git branch --show-current
git status --short
```

Expected branch: part-b/abstraction. Open that folder in B's editor. If setting up elsewhere and no B worktree exists, create one from the repository root with `git worktree add ../symctrl-b part-b/abstraction`. If Git says it exists/is in use, inspect git worktree list rather than forcing it.

On another computer, A can clone its branch only after the coordinator has authorised sharing and made it available at the chosen remote:

```sh
git clone --branch part-a/discretization https://github.com/AnassFer/Formal-Control-System.git symctrl-a
cd symctrl-a
git branch --show-current
```

Expected branch: part-a/discretization. Open symctrl-a there. Nothing is published merely because a local branch exists. A local bundle transfer is another sharing route; [git-workflow.md](docs/git-workflow.md) explains current/future arrangements. This documentation task performs no push.

If a repository has the agreed tag but genuinely lacks local A/B branches, the coordinator can create only the missing names from that tag. Existing branches with commits must be preserved. Do not switch branches in a folder another session is using.

## 8. A's implementation order

A's responsibilities/public signatures remain unchanged.

| Order | File under src/part_a/ | Task |
|---|---|---|
| 1 | uniform_grid.cpp | Generic grid, IDs/indices, geometry, locate and closed intersections |
| 2 | input_catalog.cpp | Explicit and Cartesian inputs in agreed order |
| 3 | policy_adapter.cpp | Apply a supplied memoryless/finite-memory policy and return owned input |

factories.cpp is reserved coordination space; avoid duplicate definitions. Read tests/part_a/part_a.cpp and shared contracts first. Add optional private headers under include/symctrl/part_a/ and notes/tests in owned trees. Keep dimensions configurable; preserve point ownership, closed geometry, OUT handling, validation and overflow checks. A does not compute physical reachable sets or generate a policy.

From A's checkout root:

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_a --output-on-failure
python3 tools/check_readiness.py build/dev --mode a
python3 tools/check_ownership.py auto
```

## 9. B's implementation order

B starts with the general backend, not a named example.

| Order | File under src/part_b/ | Task |
|---|---|---|
| 1 | monotone_reachability.cpp | Endpoint enclosure using a supplied model's justified capability |
| 2 | derivative_reachability.cpp | Full-box absolute-Jacobian enclosure for general dimensions |
| 3 | factories.cpp | Select backend through makeReachability and retain injected dynamics |
| 4 | explicit_transition_system.cpp | Owned sparse graph and compact universal successor sets |
| 5 | model_builder.cpp | Build transitions from public grid/input/enclosure views and assemble bundles |
| 6 | model_export.cpp | Complete generic file-package export |

Read tests/part_b/part_b.cpp and the contracts. Test through fake A views and independent generic dynamics, so B need not wait for A. Enclose every state/disturbance in a closed cell, not only its centre. Validate capabilities, dimensions, IDs, boundary intersections, graph ownership and storage budgets. AllStates means every ID 0..M; empty Explicit is invalid producer output, not a safe choice.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_b --output-on-failure
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_ownership.py auto
```

The original eight B cases include legacy fixture coverage; extend meaningful generic cases during implementation. No production algorithm is added by this documentation task. See [B's notes](docs/part_b/README.md).

## 10. Part 2's starting base

Read [PART2_HANDOFF.md](PART2_HANDOFF.md). First open examples/part2_fixture_consumer.cpp and learn the ModelBundle/transition/policy views. Run the fixed consumer without A/B libraries:

```sh
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
```

There are nine passing contract/handoff checks in this mode. They validate fixed data/interfaces, not synthesis algorithms.

A useful generic graph is fixtures::nondeterministicChoice() from fixture.hpp. It has states 1,2,3 and OUT 0. At state 1, action 0 reaches {2,3}, action 1 reaches {2}. For safe={1,2}, action 1 is the manually checked robust choice because 3 is unsafe. The separate specification.json records safe/target sets; this is test data rather than computed synthesis output.

Part 2 later implements general synthesis against graphs/specifications and returns ISymbolicPolicy. Agree specification/result/factory APIs before adding shared declarations. Future safety/reachability or finite-automaton support must stay model-independent. Use part2 headers/sources/tests/docs and local CMake files. Enable the reserved workspace with:

```sh
cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON
cmake --build build/part2 --parallel 2
ctest --test-dir build/part2 --output-on-failure --no-tests=error
```

symctrl_part2_workspace is currently an INTERFACE target; add normal compiled targets when sources are created. No algorithm-readiness suite exists yet. Same/separate repository and file-format routes are in [the quickstart](docs/part2-quickstart.md). The old generated archive has historical guidance; use the current handoff document.

## 11. Understand test results and save work

The original full scaffold has 9 passes and 14 skips: 5 A, 8 B and 1 integration. Only NotImplemented permits skipping. CTest's aggregate percentage can include skips; a pending case is not completed work. Readiness must reject skipped/missing/failed/disabled required tests. Current coverage is not a full-paper or robot acceptance suite.

Build first, run your labelled tests, run ownership, inspect git diff and save meaningful commits in your own branch. For B, for example:

```sh
git branch --show-current
git status --short
git diff
git add -- include/symctrl/part_b src/part_b tests/part_b docs/part_b
git commit -m "Describe the actual B change"
```

A uses matching part_a paths; Part 2 uses part2. A commit is local. Remote sharing requires a separately authorised push. Shared coordinator Markdown changes do not belong in an owner's feature commit.

## 12. Integration and the later robot example

When A/B are independently ready, the coordinator normally merges them into a separate integration checkout and runs the required cross-part tests. A clean textual merge alone is insufficient. Part 2 later consumes real bundles through the same boundary and gets joint synthesis/refinement tests.

The mobile robot is then an end-to-end example of the completed general engine. Put its dynamics, parameters, labels and periodic-heading handling in model/example adapters. Do not turn the graph builder or synthesis algorithm into robot-specific code. Nothing in this documentation update implements that example.

For details use [contracts](docs/contracts.md), [the blueprint](docs/part1-final-structure.md), [status](docs/implementation-status.md), [source map](docs/source-map.md), [Git workflow](docs/git-workflow.md) and [handoff rules](docs/part1-to-part2.md).
