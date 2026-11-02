# Start here: a beginner's guide to the project

Read this first if you are unsure what the folders mean, which files to edit, or how your work connects to your teammates' work.

The project has **two people working on Part 1** and **a separate group working on Part 2**. They can work at the same time. This guide explains their future work; it does not mean those features already exist.

## 1. What are we trying to build?

Imagine a room whose temperature is 18°C. A heater can be OFF or ON. The outside temperature changes, so switching the heater ON does not always produce exactly the same next temperature.

Instead of working with every possible decimal temperature, we group temperatures into numbered cells:

| State ID | Temperatures owned by that state |
|---|---|
| 1 | 17°C up to, but not including, 19°C |
| 2 | 19°C up to, but not including, 21°C |
| 3 | 21°C through 23°C, including 23°C |
| 0 | Outside the chosen temperature domain |

For example, 18°C belongs to state 1, 19°C belongs to state 2, and 24°C is outside, represented by state 0.

Our program will answer questions such as: **“Starting anywhere in state 1, if the heater is ON, which states could come next?”** The answer can contain several states because the exact initial temperature and disturbance vary.

The work is divided as follows:

| Person/group | Main question to answer | What they produce |
|---|---|---|
| Person A | Which cell contains this temperature, and which physical input does an action mean? | State grid, input catalogue and policy adapter |
| Person B | From each cell and input, which cells could come next? | Dynamics, reachable enclosures and a transition graph/model package |
| Part 2 group | Given this graph and a goal, which action should we choose? | A symbolic control policy |
| Coordinator | Do the pieces still agree and work together? | Shared contract updates and the integrated project |

The coordinator can be one of the students wearing a second role. It does not require a fourth person. The important point is that shared changes are agreed once rather than made differently by each person.

**Part 1 builds the model. Part 2 uses that model to choose actions.** Person A's adapter later turns Part 2's chosen action into a physical input for the system.

The article's Section 1 defines Part 1. The slides also discuss controller synthesis and a largest-building competition, which go beyond Part 1. See [the source map](docs/source-map.md) for the exact mathematical scope.

## 2. What is already done, and what is still missing?

The repository currently contains a foundation: agreed interfaces, small common helpers, empty production implementations, tests, examples and documentation.

| Item | Current state |
|---|---|
| Shared C++ interfaces and file format | Written and versioned |
| Validation and successor-set helpers | Working |
| Two tiny hand-written example models | Working fixtures |
| Example program reading a fixture | Working |
| Person A's production features | Unimplemented stubs |
| Person B's production features | Unimplemented stubs |
| Part 2's controller algorithms | Not started |

A **stub** is a function with the correct name and arguments whose body currently throws `symctrl::NotImplemented`. It marks the place where the real implementation belongs.

A **fixture** is a tiny, fixed example written by hand so other people can develop and test their code. It is not output from a completed Part 1 algorithm.

The original common starting point is:

```text
Tag:    part1-contract-v1
Commit: 25c6fe909cac531c2a3e1d2593cd598ff4522064
```

The local branches `main`, `part-a/discretization` and `part-b/abstraction` were all created at this commit. Nothing was pushed during initialisation. The beginner guide you are reading is a later coordinator documentation addition; the original tag continues to identify the original foundation.

## 3. A few words you will see everywhere

| Word | Meaning here |
|---|---|
| Repository / repo | The project files together with their Git history |
| Repository root | The top project folder containing `README.md`, `CMakeLists.txt` and `.git/` |
| Branch | A named line of development, such as Person A's work |
| Commit | A saved snapshot of changes in local Git history |
| Tag | A name for a particular saved commit; our tag marks the agreed common foundation |
| Clone | A separate copy of a Git repository, usually on a person's own computer |
| Worktree | Another folder connected to the same local Git repository, with its own checked-out branch |
| Push | Sending local commits/refs to a remote server such as GitHub |
| Interface / contract | The agreed operations and meanings that other code can rely on |
| Factory | A function that creates an object, such as `makeUniformGrid(...)` |
| Build | Compile the C++ source into libraries and executable programs |
| Test | Run checks that compare behaviour with expected results |
| Transition / successor | A possible next symbolic state after choosing an input |
| Policy | A rule that chooses an action from the current state and possibly some memory |
| SDK | The shared headers/helpers/examples that another group can build against |

In C++, `.hpp` files normally describe types and operations; `.cpp` files contain implementations. Here the shared `.hpp` files are the agreed public contract, while the owner-specific `.cpp` files are where most implementation work will happen.

## 4. What is inside each folder?

Paths in this guide are relative to the repository root unless shown as an absolute path beginning with `/`.

```text
Automate Project/
├── START_HERE.md                 This guide
├── README.md                     Project overview and standard commands
├── AGENTS.md                     Rules for coding assistants
├── CONTRIBUTING.md               Rules for contributors
├── CMakeLists.txt                Shared top-level build setup
├── CMakePresets.json             Named build configurations
├── include/symctrl/
│   ├── contracts/                Shared public interfaces
│   ├── part_a/                   A's optional private headers
│   ├── part_b/                   B's optional private headers
│   └── part2/                    Part 2's headers
├── src/
│   ├── common/                   Completed small shared helpers
│   ├── part_a/                   A's production implementation
│   ├── part_b/                   B's production implementation
│   └── part2/                    Part 2's future implementation
├── tests/                        Automated checks, separated by owner/purpose
├── fixtures/                     Fixed examples used before real algorithms exist
├── examples/                     Small programs and model configurations
├── apps/                         Future production command-line program
├── schemas/                      Rules for the model-package file format
├── docs/                         Explanations, contracts and workflow details
├── tools/                        Ownership, readiness and fixture-check scripts
├── .github/workflows/            Hosted CI configuration
├── Project files/                Local source PDFs; not committed
├── build/                        Generated build output; not committed
└── .git/                         Git's internal data; leave it to Git
```

Some generated/local folders appear only after setup or a build. The PDFs are available in the original workspace; they are ignored and will not automatically appear in a clone.

### The shared headers: learn the names, keep the agreement

| File in `include/symctrl/contracts/` | What it describes |
|---|---|
| `types.hpp` | IDs, vectors, boxes, grid settings, exceptions and validation helpers |
| `grid.hpp` | How to ask for a cell ID, cell bounds or intersecting cells |
| `inputs.hpp` | How to turn an action ID into its physical input vector |
| `dynamics.hpp` | How the physical system moves one step and exposes mathematical bounds |
| `reachability.hpp` | How to request a box enclosing all possible next physical states |
| `transitions.hpp` | How to ask for possible next symbolic states |
| `policy.hpp` | How a symbolic policy returns its chosen action and updated memory |
| `model_bundle.hpp` | How grid, inputs, transitions and metadata are packaged together |
| `factories.hpp` | The exact public creation/refinement/export function declarations |

Think of these files as the agreed plugs and sockets. Each person implements their side so the existing connections fit. If a signature or meaning really needs to change, explain the problem to the coordinator first. Read [contracts.md](docs/contracts.md) for exact behaviour.

### Tests: which checks are for whom?

| Folder | Purpose | Who normally edits it? |
|---|---|---|
| `tests/contracts/` | Shared types, validation and representation checks | Coordinator |
| `tests/part_a/` | Real A implementation checks | A |
| `tests/part_b/` | Real B implementation checks using fake A dependencies | B |
| `tests/integration/` | Real A and B together | Coordinator; assertions are already written |
| `tests/part2_handoff/` | Fixture/public-interface compatibility checks | Coordinator |
| `tests/part2/` | Part 2's future algorithm checks | Part 2 group |
| `tests/support/` | Reusable test helpers and independent expected results | Coordinator |

### Examples, fixtures, tools and generated files

| Location | What to use it for |
|---|---|
| `fixtures/support/fixture.hpp` and `fixture.cpp` | Obtain tiny fixed C++ models through the public interfaces |
| `fixtures/part1_v1/thermostat/` | Read the complete hand-written thermostat JSON/CSV model |
| `fixtures/part1_v1/nondeterministic_choice/` | Read a tiny model that demonstrates why every successor matters |
| `examples/part2_fixture_consumer.cpp` | First working C++ example for the Part 2 group to study |
| `examples/standalone/` | Working example of a separate CMake project using this repo as an SDK |
| `examples/thermostat.json`, `building-n3.json` | Model settings to read; no general JSON config loader exists yet |
| `apps/main.cpp` | Future production CLI; it currently encounters unimplemented factories |
| `schemas/part1-model-v1.schema.json` | Structural rules for the exported manifest |
| `tools/check_ownership.py` | Check whether your changes stay inside your assigned folders |
| `tools/check_readiness.py` | Check whether required tests exist and pass with zero skips |
| `tools/check_fixture_packages.py` | Check the tiny fixture packages' format and consistency |
| `build/` | Compiler output, executables and test logs; do not edit or commit these |
| `exports/`, `benchmarks/`, `out/` | Ignored places for generated results; create them when needed |

## 5. Exactly where may each person work?

| Role | Branch | Allowed paths |
|---|---|---|
| A | `part-a/discretization` | `include/symctrl/part_a/`, `src/part_a/`, `tests/part_a/`, `docs/part_a/` |
| B | `part-b/abstraction` | `include/symctrl/part_b/`, `src/part_b/`, `tests/part_b/`, `docs/part_b/` |
| Part 2 group | A later branch such as `part2/controller` | `include/symctrl/part2/`, `src/part2/`, `tests/part2/`, `docs/part2/` |
| Coordinator | Shared foundation/integration work | Shared interfaces, root build/docs, fixtures and integration/handoff checks |

Each person can read the whole repository. Their feature changes belong in their assigned four folders. Shared code and examples stay common references.

The root build already includes each owner's local `CMakeLists.txt`. Add new implementation files to `src/part_a/CMakeLists.txt`, `src/part_b/CMakeLists.txt` or `src/part2/CMakeLists.txt`; add tests through the matching local test CMake file. Owners do not need to change the root `CMakeLists.txt` for ordinary source additions.

## 6. Before starting: tools and terminal location

You need Git, a C++17 compiler, CMake version 3.21 or later, and Make for the supplied presets. CTest comes with CMake. Python 3 is used for the optional ownership/readiness scripts, not for the C++ algorithms.

You can check the tools with:

```sh
git --version
g++ --version
cmake --version
ctest --version
python3 --version
```

On the original machine, CMake was temporarily installed under `/tmp` during initialisation. If it still exists, this makes it available in the current terminal:

```sh
export PATH="/tmp/symctrl-cmake/cmake/data/bin:$PATH"
cmake --version
```

That temporary directory may disappear later. In that case, provide a normal CMake installation before building. This temporary-path command is specific to this machine, not a requirement for everyone's computer.

Before running project commands, enter the correct checkout folder. `pwd` prints your current folder. The original checkout is:

```sh
cd "/home/ilyazz/Automate Project"
pwd
git branch --show-current
git status --short
```

Quotes are needed around that path because it contains a space. When this guide says “run from your checkout root,” it means the folder containing that checkout's `CMakeLists.txt`.

## 7. How to initialise and open the branches

**A and B's branches already exist locally. Do not run `git init` again or recreate them.** Creating a branch and opening a working folder for it are separate operations.

Choose one setup route below. Use separate folders for separate people or concurrent coding sessions.

### Route A: same computer — use worktrees

From the original checkout, the coordinator runs these once, provided the destination folders do not already exist:

```sh
cd "/home/ilyazz/Automate Project"
git branch --list
git worktree add ../symctrl-a part-a/discretization
git worktree add ../symctrl-b part-b/abstraction
```

This produces:

| Folder | Branch | Who uses it? |
|---|---|---|
| `/home/ilyazz/Automate Project` | `main` | Shared reference/coordinator |
| `/home/ilyazz/symctrl-a` | `part-a/discretization` | A |
| `/home/ilyazz/symctrl-b` | `part-b/abstraction` | B |

A opens `/home/ilyazz/symctrl-a` in their editor and terminal. B opens `/home/ilyazz/symctrl-b`. The files have the same relative names, but the folders contain separate working copies. Each also gets its own `build/dev` when built.

For example, A checks their location with:

```sh
cd /home/ilyazz/symctrl-a
git branch --show-current
```

Expected output: `part-a/discretization`. B does the same in `/home/ilyazz/symctrl-b` and expects `part-b/abstraction`.

If Git says a branch is already checked out in another worktree, run `git worktree list` and use that folder. Do not force the checkout. A worktree folder is not a remote share for someone on another computer.

### Route B: different computers — use separate clones after sharing

The foundation has not been pushed by this session. The following commands work only after the coordinator has deliberately shared the baseline tag and branches at the chosen remote. A remote URL being configured does not mean the new branches are already available there.

After that sharing, A runs on their own computer:

```sh
git clone --branch part-a/discretization https://github.com/AnassFer/Formal-Control-System.git symctrl-a
cd symctrl-a
git branch --show-current
```

B runs on their own computer:

```sh
git clone --branch part-b/abstraction https://github.com/AnassFer/Formal-Control-System.git symctrl-b
cd symctrl-b
git branch --show-current
```

These clones have their own Git history/files and track their respective remote branches. If the remote branch cannot be found, first confirm that it has actually been shared. The coordinator can also transfer a local Git bundle without pushing; the detailed sharing plan belongs in [git-workflow.md](docs/git-workflow.md).

If you already have a clone, check `git status --short` first and keep unfinished edits safe. Once clean, fetch the shared refs with `git fetch origin --tags`. If your local branch exists, use `git switch part-a/discretization` or the B equivalent. If only `origin/part-a/discretization` exists, `git switch --track origin/part-a/discretization` creates its local tracking branch. Use the B name for B. Do not switch branches inside a folder another session is using.

### Only if a branch is genuinely missing: create it from the tag

This is a recovery/setup explanation, not something to repeat in the current original repo. If the common tag is available and the two local branches do not exist, create them with:

```sh
git branch part-a/discretization part1-contract-v1
git branch part-b/abstraction part1-contract-v1
```

These commands create names; they do not switch your current folder. Both start from the agreed tag. Do not recreate branches that already contain someone's work. The setup instructions above then open the branch in a working folder.

## 8. Person A: what to do, in order

**Your code starts in `src/part_a/`. Your main test source is `tests/part_a/part_a.cpp`.** Read the tests and shared interfaces before replacing a stub.

| Order | File to work on | What to implement | What should become possible? |
|---|---|---|---|
| 1 | `src/part_a/uniform_grid.cpp` | `makeUniformGrid(...)` and an implementation of `IStateGrid` | Create a grid, convert IDs/indices, obtain closed cell boxes, locate a point and find intersecting cells |
| 2 | `src/part_a/input_catalog.cpp` | `makeInputCatalog(...)`, `makeCartesianInputs(...)` | Obtain the physical input vector for an action ID, in the agreed order |
| 3 | `src/part_a/policy_adapter.cpp` | `refineDecision(...)` | Convert a measured state plus supplied policy into an owned physical input and updated memory |
| Alongside these | `tests/part_a/`, `docs/part_a/` | Extend checks and explain your implementation | Other people can understand and verify A independently |

`src/part_a/factories.cpp` is currently reserved coordination space. The existing factory definitions live in the module files listed above; do not define the same function twice.

Concrete examples of A's responsibilities:

* `locate({18})` on the thermostat grid returns state 1.
* `locate({19})` returns state 2; internal boundary ownership is exact.
* `inputs.value(1)` returns the selected physical vector, `[1]` in this example.
* If a supplied policy chooses action 1 in state 1, the adapter returns that action and its physical vector. The adapter does not decide the policy itself.

Keep grid dimensions general: the building has N coordinates. Follow dimension-0-fastest ordering, validate invalid inputs/IDs and check integer overflow. OUT cannot be given a finite cell box, and the adapter must explicitly reject an outside measured state.

A does not need B to finish first. A does not implement physical dynamics, reachable sets, graph generation or controller synthesis.

From A's checkout root, run:

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_a --output-on-failure
python3 tools/check_readiness.py build/dev --mode a
python3 tools/check_ownership.py auto
```

During early development, A tests may still skip on remaining stubs. A is ready for integration when the A readiness command passes with zero required skips, ownership passes and your implementation notes are saved. B/integration tests can still be pending on A's isolated branch.

## 9. Person B: what to do, in order

**Your code starts in `src/part_b/`. Your main test source is `tests/part_b/part_b.cpp`.** The tests already use fixed fake grid/input objects so you can work without A's completed code.

| Order | File to work on | What to implement | What should become possible? |
|---|---|---|---|
| 1 | `thermostat_dynamics.cpp` | `makeThermostatDynamics()` and `IDynamics` behaviour | Evaluate the thermostat's physical next state and expose justified mathematical capabilities |
| 2 | `monotone_reachability.cpp`, `derivative_reachability.cpp`, `factories.cpp` | Both `IReachability` backends and `makeReachability(...)` dispatch | Enclose all possible next physical states from a cell and disturbance box |
| 3 | `ring_building_dynamics.cpp` | `makeRingBuildingDynamics(N)` | Evaluate the fixed periodic building model, with no separate disturbance |
| 4 | `explicit_transition_system.cpp`, `model_builder.cpp` | Sparse immutable storage, `buildSymbolicModel(...)`, `assembleModelBundle(...)` | Query all successor sets and deliver a complete bundle |
| 5 | `model_export.cpp` | `exportModelPackage(...)` | Write the complete manifest/cells/inputs/transitions package |
| Alongside these | `tests/part_b/`, `docs/part_b/` | Extend checks and document numerical assumptions, limits and measurements | B can be verified independently |

All abbreviated filenames in this table are inside `src/part_b/`.

For example, B must enclose the thermostat's next temperatures from the **whole** closed cell [17,19], with the heater ON and outside temperature anywhere in [10,15]. The expected enclosure is [18.9,20.8], which intersects cell closures 1 and 2. The expected successor set is therefore `{1,2}`.

Simulating only 18°C would miss the requirement to cover every starting point in the cell. Both the mathematics and floating-point limitations are explained in [source-map.md](docs/source-map.md).

B's tests obtain fake A views through `fixtures::thermostat()` in `fixtures/support/fixture.hpp`. Read those public views, but do not change shared fixtures to fit your implementation. Production B must not depend on fixture support or on A's private implementation.

Store OUT's successor set as compact `AllStates`. Handle closed-boundary intersections, reject invalid/empty producer results and enforce pair/edge/storage limits. Ordinary `double` arithmetic remains `UncertifiedFloat`; passing tests does not upgrade it to a certified enclosure.

From B's checkout root, run:

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_b --output-on-failure
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_ownership.py auto
```

B is ready when its required tests run without skips, including the fixture-based checks, and ownership passes. A/integration tests can still be pending on B's isolated branch. Start with the tiny examples; the later official building benchmark must retain its fixed model and discretisation.

## 10. Part 2: the exact starting point

**Start by opening `examples/part2_fixture_consumer.cpp`. Start your own implementation later under `src/part2/`, or in your separate repository.**

The example already shows how to obtain a model and ask questions through the agreed interfaces. It works now, even though all real A/B factories are unfinished.

### First session: run the working example

From a checkout containing the common foundation:

```sh
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
```

The `sdk` preset turns off A/B production libraries and the production CLI. The nine foundation/handoff tests should pass without skips. The example prints lines including:

```text
model=thermostat_fixture
successors(1,0)=explicit: 0 1
successors(0,0)=all: 0 1 2 3
fixture only; no generation or synthesis
```

These are selected lines/fields from the output. The complete expected output is in `fixtures/part1_v1/thermostat/expected-output.txt`.

Study these files next, in this order:

1. `fixtures/support/fixture.hpp`: the two ready-to-use fixture-bundle functions.
2. `include/symctrl/contracts/model_bundle.hpp`: the pieces supplied in a model.
3. `include/symctrl/contracts/transitions.hpp`: successor queries and utilities.
4. `fixtures/part1_v1/nondeterministic_choice/specification.json`: a separate task to apply to the example graph.
5. `include/symctrl/contracts/policy.hpp`: the policy interface to return later.
6. [part1-to-part2.md](docs/part1-to-part2.md): full semantics and file-format rules.

### What Part 2 receives

The C++ `ModelBundle` contains:

| Member | What Part 2 can ask |
|---|---|
| `grid` | What are this state's closed bounds? Which state owns a physical point? |
| `inputs` | What physical vector does this action ID mean? |
| `transitions` | What are all possible successors for this state and action? |
| `metadata` | What model/version/dimensions/counts/provenance/numerical status am I using? |

For example, these are the kinds of queries to learn from the existing consumer:

```cpp
const auto model = symctrl::fixtures::nondeterministicChoice();
const auto& successors = model.transitions->successors(1, 0);
const bool can_reach_state_3 = symctrl::contains(
    successors, 3, model.metadata.internal_states);
```

This example requires `fixture.hpp` and linking fixture support, as the existing consumer does. It only queries a model; it does not generate a controller.

### First small problem to understand

In the nondeterministic-choice fixture:

```text
Safe states:   {1,2}
Target states: {2}

At state 1:
  action 0 may go to 2 OR 3
  action 1 goes to 2
```

Action 0 can reach unsafe state 3, so the manually checked robust choice at state 1 is action 1. Your future algorithm must account for **every possible successor**, not just find one favourable edge. The model and this safe/target specification are stored separately because the same model can be used for different tasks.

Three details must be understood before implementing algorithms:

* State 0 means OUT and is normally unsafe. A state ID is different from an action ID or a physical input value.
* `AllStates` means every ID from 0 through M, even though its stored ID vector is empty. Use the provided helpers to interpret it.
* An empty `Explicit` successor set indicates an incomplete/invalid producer result for these models. Do not count it as a successful action because there is nothing to check.

### Where the group creates its own files

For the same-repository arrangement, the coordinator can later create a separate Part 2 worktree from the tag. This command creates a new branch, unlike the existing A/B worktree commands:

```sh
cd "/home/ilyazz/Automate Project"
git worktree add -b part2/controller ../symctrl-part2 part1-contract-v1
cd ../symctrl-part2
cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON
cmake --build build/part2 --parallel 2
ctest --test-dir build/part2 --output-on-failure --no-tests=error
```

No Part 2 branch is created merely by reading this guide. Use the command only when starting that group's work; if that branch/worktree already exists, open it instead.

The group adds implementation files inside `src/part2/`, headers inside `include/symctrl/part2/`, tests inside `tests/part2/` and notes inside `docs/part2/`. Names such as `controller.cpp` and `controller_tests.cpp` would be new owner files, not existing completed modules.

`src/part2/CMakeLists.txt` currently declares an `INTERFACE` target called `symctrl_part2_workspace`. That target is only a connection point; it compiles no implementation. When actual `.cpp` files are added, the group adds a normal compiled library/executable through that local CMake file and can link it through the workspace target. Its local test CMake file is also ready to extend. Do not put new Part 2 algorithms in the frozen example or shared handoff tests.

Run `python3 tools/check_ownership.py part2` from that checkout to check path ownership. There is no frozen Part 2 algorithm-readiness suite yet; the group must add its own meaningful tests. The nine existing handoff tests validate the supplied example/API, not the group's future synthesis algorithm.

### If Part 2 uses another repository or language

For C++, open `examples/standalone/` and follow [the separate C++ quickstart](docs/part2-quickstart.md). It demonstrates using this checkout through CMake's `add_subdirectory`, without building A/B.

For another language, begin with the files under `fixtures/part1_v1/`:

| File | Meaning |
|---|---|
| `manifest.json` | Version, model parameters, dimensions, domain, counts and meanings of the data |
| `cells.csv` | Closed bounds of each internal state |
| `inputs.csv` | Physical input vector for each action ID |
| `transitions.csv` | Every state/action pair's possible successors |
| `specification.json` in the choice fixture | Separate safe/target task and a manually checked expectation |

The group can write a loader against these fixed files now; it does not need to wait for B's exporter. Use [the schema](schemas/part1-model-v1.schema.json) and [the handoff rules](docs/part1-to-part2.md), including rejection of missing pairs, bad counts and incompatible versions.

### What Part 2 should eventually deliver

Once the agreed synthesis work is implemented, Part 2 supplies a policy implementing `ISymbolicPolicy` for C++ integration. Its decision operation takes previous memory and current state, then returns updated memory and an action ID. Default initial memory is 0 unless the policy explicitly requires another initial value.

The policy uses the model's existing action mapping. It must report states outside its policy domain explicitly. Person A's adapter handles the later physical-input conversion; Part 2 does not need to recreate B's reachability methods or know its graph storage.

Later replace the fixture bundle with a real Part 1 bundle using the same public interfaces. Rerun compatibility checks and add joint tests: fixture success alone does not prove that the finished physical-control system is correct.

## 11. How to understand test results

At the original foundation, the full default suite has:

| Result | Number | What it means |
|---|---|---|
| Passed | 9 | Shared contracts and fixture handoff really execute successfully |
| Skipped | 14 | Five A cases, eight B cases and one integration case encounter missing implementations |
| Failed | 0 | No executed check failed in the validated baseline |

CTest may still print “100% tests passed” alongside skips. Always read the skipped-test list. A skip is pending work, not a completed feature.

The readiness scripts distinguish the stages:

| Command | When it should pass |
|---|---|
| `python3 tools/check_readiness.py build/dev --mode foundation` | Shared contracts and fixture handoff work; passes at baseline after a full build |
| `python3 tools/check_readiness.py build/dev --mode a` | A's required tests and contracts pass without skips |
| `python3 tools/check_readiness.py build/dev --mode b` | B's required tests, contracts and fixture handoff pass without skips |
| `python3 tools/check_readiness.py build/dev --mode integration` | Contracts, handoff, A, B and real A+B integration all pass without skips |

Run readiness after building, because missing executables are not passing tests. A real test failure should lead to an implementation fix; do not remove the check or convert arbitrary exceptions into skips.

## 12. A simple daily Git routine

Run this from your own feature checkout before editing:

```sh
git branch --show-current
git status --short
```

Confirm your branch, edit your own files, build, run your tests and inspect the diff with `git diff`. The ownership check covers committed changes, staged/unstaged edits and nonignored new files relative to the common tag. It catches shared-file renames too.

When saving an A change, stage only A's paths:

```sh
git add -- include/symctrl/part_a src/part_a tests/part_a docs/part_a
git commit -m "Describe the actual A change here"
```

B uses the matching four `part_b` paths; Part 2 uses `part2`. Replace the sample commit message with a useful description. A commit saves locally; it does not publish anything. Remote sharing is a separate coordinated step, and this documentation task performs no push.

Use small meaningful commits while developing. Tell the coordinator that a part is ready only after its readiness and ownership gates pass. If a new shared API is needed, describe the concrete problem and proposed change before building incompatible alternatives.

## 13. Who combines the finished work?

After both A and B are ready, the coordinator creates a separate integration worktree, normally merges both completed branches, and runs the strict integration gate plus the real thermostat demonstration. The integration test already contains the six-row oracle and the cross-part assertions.

Neither student needs to merge the other's unfinished branch to start their own implementation. The coordinator does not assume a merge is correct just because Git reported no text conflicts. Full later commands are in [git-workflow.md](docs/git-workflow.md).

Once the producer is integrated, the Part 2 group can consume its real bundle/package. Joint Part 1/Part 2 algorithm tests happen when both sides exist. A/B/Part 2 feature work belongs on its corresponding branch rather than on `main`.

## 14. If you get stuck

| What you see | What to do |
|---|---|
| `cmake: command not found` | Check the tool setup in section 6; the original machine's temporary CMake path is not permanent |
| `not implemented` or a skipped feature test | Check the relevant owner's remaining stub; this is expected at the starting foundation |
| Ownership checker fails | Read the listed paths and confirm you are in your feature checkout. Shared coordinator documentation edits do not belong in an owner's feature commit |
| `auto` cannot detect a work stream on `main` | Open your A/B/Part 2 checkout before running the feature ownership audit |
| Remote branch not found | Confirm the branch has actually been shared; local branches are not automatically on GitHub |
| Worktree destination already exists / branch already in use | Run `git worktree list` and inspect the existing folder; do not delete or force it |
| CMake refers to an old folder/compiler | Use a new build directory for the new checkout/configuration rather than copying another person's build tree |
| You think a shared contract is wrong or insufficient | Bring the exact failing example and proposed change to the coordinator |

For a deeper explanation, use these documents:

* [Exact public interfaces and conventions](docs/contracts.md)
* [Detailed finished Part 1 file blueprint](docs/part1-final-structure.md)
* [Baseline implementation-status list](docs/implementation-status.md)
* [Mathematics, source scope and thermostat oracle](docs/source-map.md)
* [Branch, sharing and later merge commands](docs/git-workflow.md)
* [Part 2 quickstart](docs/part2-quickstart.md)
* [Part 1 → Part 2 API/file-format agreement](docs/part1-to-part2.md)
* [What was actually validated during initialisation](docs/bootstrap-validation.md)
