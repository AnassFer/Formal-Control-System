# Part 2: a base for general symbolic-controller synthesis

Part 1 is being implemented in A/B tracks. It produces a general symbolic transition model and runtime refinement interface. Part 2 will use that model plus a separate specification to synthesise a policy. Keep both stages independent of a particular physical model. A mobile robot is a later end-to-end example after the engine is ready.

This document supersedes earlier handoff guidance. The original part1-contract-v1 tag, commit 25c6fe909cac531c2a3e1d2593cd598ff4522064, identifies the existing scaffold. Public contract/schema version remains 1.0.0. No algorithm or API is changed by this documentation revision. The earlier generated archive contains historical guidance and should not be forwarded as the current plan.

## 1. Run the existing base

From the repository root, with a C++17 compiler, CMake >=3.21 and Make:

```sh
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
```

The SDK preset excludes A/B production libraries. The original foundation has nine passing contract/handoff checks, zero skips in this mode, and a working fixture consumer. Fixtures are hand-written data, not completed Part 1 output. Part 2 synthesis remains unimplemented; these checks do not test a future controller algorithm.

## 2. Files to study first

| File | Purpose |
|---|---|
| examples/part2_fixture_consumer.cpp | Working example of metadata, geometry/input and successor queries |
| fixtures/support/fixture.hpp | Tiny fixed model sources; use fixtures::nondeterministicChoice() for the general graph below |
| include/symctrl/contracts/model_bundle.hpp | Grid, catalogue, graph and metadata handed to consumers |
| include/symctrl/contracts/transitions.hpp | Successor representation and membership/nonempty/iteration helpers |
| include/symctrl/contracts/policy.hpp | Existing policy decision/memory interface |
| fixtures/part1_v1/nondeterministic_choice/specification.json | Separate task with safe/target sets and a manually checked expectation |
| docs/part1-to-part2.md | Complete current API/file-format handoff |
| docs/contracts.md | Existing signatures, dimensions, errors and lifetimes |

## 3. What you receive and eventually return

ModelBundle holds immutable shared grid/input/transition views plus owned metadata. Graph queries accept state IDs 0..M and action IDs 0..A-1. Physical vectors come from the input catalogue. A model identifier is metadata, not a branch condition for controller logic.

Your eventual synthesis layer receives a model and separate specification, computes a winning domain/policy, and exposes the policy through ISymbolicPolicy. Its decide(previous_memory,current_state) operation returns next_memory and input. Initial memory defaults to 0; other requirements must be explicit. Missing entries/invalid memory fail explicitly. A's adapter later converts the chosen action to its physical vector.

Exact public specification/result/synthesis factories are not declared yet. Agree them in one common contract update before dependent implementation. Existing model/policy interfaces are a base, not an already implemented synthesis API.

## 4. First graph to understand

```text
Regular states: 1,2,3; OUT: 0; actions: 0,1
Safe set: {1,2}; target set: {2}
(1,0) -> {2,3}
(1,1) -> {2}
(2,0) -> {2}; (2,1) -> {2}
(3,0) -> {3}; (3,1) -> {3}
(0,0) -> AllStates; (0,1) -> AllStates
```

At state 1, action 1 is the manually checked robust choice. Action 0 can also reach unsafe state 3. Your future solver must reason about every successor; one favourable edge/path is insufficient. This expectation is fixed test data, not computed synthesis output.

Use contains, nonempty and forEachSuccessor. AllStates means every ID 0..M even though its stored vector is empty. An empty Explicit set is invalid/incomplete producer output for current complete models and must not create vacuous control success. OUT is normally unsafe and has no finite cell box. All selected actions are enabled everywhere in the current contract.

Geometry/intersections use closed bounds, while point ownership uses half-open intervals with the last upper endpoint included. Whole-cell inclusion is normally required for conservative region-safety labels. Ordinary numerical results remain uncertified.

## 5. Where your group works

```text
include/symctrl/part2/   Headers
src/part2/              Algorithms and local CMakeLists.txt
tests/part2/            Algorithm tests and local CMakeLists.txt
docs/part2/             Notes and algorithm documentation
```

Enable the reserved subtree with:

```sh
cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON
cmake --build build/part2 --parallel 2
ctest --test-dir build/part2 --output-on-failure --no-tests=error
```

symctrl_part2_workspace is currently an INTERFACE target with no implementation. Add a compiled library/executable and tests through the local files when algorithm work is authorised. Preserve shared headers, fixture packages and handoff assertions. Coordinate public changes with both Part 1 owners.

For Git work, obtain the shared tag through a clone/bundle after authorised sharing and create your group branch from that baseline. A source ZIP has no Git history. No branch or algorithm is created by this documentation task. Ownership checks require an available agreed comparison ref.

## 6. Separate repository or language

For C++, [examples/standalone/](examples/standalone/) demonstrates using the checkout as an SDK without A/B libraries:

```sh
cmake -S examples/standalone -B build/external -DSYMCTRL_SDK_SOURCE="$PWD"
cmake --build build/external --target external_fixture_client --parallel 2
./build/external/external_fixture_client
```

For another language, start from the generic choice package in fixtures/part1_v1/nondeterministic_choice/. It contains manifest.json, cells.csv, inputs.csv, transitions.csv and a separate specification.json. Use [the schema](schemas/part1-model-v1.schema.json) and [the serialization rules](docs/part1-to-part2.md). Reject bad IDs/counts/shapes, missing pairs, duplicate or mixed all/explicit rows and incompatible versions.

Later replace the fixture source with real Part 1 output through the same boundary. Add joint algorithm checks once implementations exist. See [the quickstart](docs/part2-quickstart.md) for independent build/Git arrangements.
