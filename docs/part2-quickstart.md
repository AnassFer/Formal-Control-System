# Independent Part 2 quickstart

Use contract/schema version 1.0.0 at `part1-contract-v1`. The tag/branches are local after initialisation; obtain a clone, local bundle/archive or authorised remote share before asking another machine to fetch them. Nothing was pushed by bootstrap. These instructions consume fixtures and public interfaces only; there is no synthesis implementation to run yet.

## Get and validate the SDK

After authorised sharing, a same-machine local clone or a shared remote works:

```sh
git clone --branch part1-contract-v1 '/home/ilyazz/Automate Project' symctrl-sdk
cd symctrl-sdk
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
python3 tools/check_readiness.py build/sdk --mode foundation
```

The local clone path is specific to this workspace; on another machine replace it with the shared clone URL or local path you actually received. For the current checkout, start directly at `cmake --preset sdk`. Result: nine contracts/handoff tests pass, zero skips, with no A/B production targets built or linked. The fixture consumer's full expected output is `fixtures/part1_v1/thermostat/expected-output.txt` and the `handoff.consumer` test compares it byte-for-byte. This proves fixture-contract usability, not real Part 1 integration readiness.

## Same repository arrangement

The group may later create its own branch from the common tag (not created during bootstrap):

```sh
git switch -c part2/controller part1-contract-v1
cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON
cmake --build build/part2 --parallel 2
ctest --test-dir build/part2 --output-on-failure --no-tests=error
python3 tools/check_ownership.py part2
```

Only `include/symctrl/part2/`, `src/part2/`, `tests/part2/`, `docs/part2/` are reserved for this group. Local CMakeLists are already included behind `SYMCTRL_BUILD_PART2=ON`; target `symctrl_part2_workspace` currently has no source/algorithm. Add future sources/tests in those local files, not frozen root files. Keep task specifications separate from model metadata.

## Separate C++ repository arrangement

Public handoff contents are `include/symctrl/contracts/`, `src/common/`, `fixtures/support/`, `fixtures/part1_v1/`, `schemas/`, the root CMake support/options and `examples/part2_fixture_consumer.cpp`; retain their common tag/version. Copying only public headers is sufficient for declarations, but calls to representation helpers require `symctrl_common`. Use the shared checkout via add_subdirectory instead of manually copying private producer sources.

The standalone project under `examples/standalone/` is a tested independent CMake consumer. Copy that directory to your own repository and point `SYMCTRL_SDK_SOURCE` at the common checkout:

```sh
cmake -S examples/standalone -B build/external -DSYMCTRL_SDK_SOURCE="$PWD"
cmake --build build/external --target external_fixture_client --parallel 2
./build/external/external_fixture_client
```

Actual output: `nondeterministic_choice_fixture M=3 unsafe_successor_3=1`. The project disables production/tests/Part 2 in the SDK subtree and links `symctrl_fixture_support`, which transitively supplies common/contracts. It builds no A/B libraries. For a declaration-only dependency, `symctrl_contracts` is an INTERFACE target; `symctrl_common` supplies completed validation and successor operations. There is no install/package manager or network dependency.

## Separate-language arrangement and later substitution

Give the group both complete package directories in `fixtures/part1_v1/`, `schemas/part1-model-v1.schema.json` and `docs/part1-to-part2.md`. The thermostat package covers all six internal transitions and universal OUT rows. The nondeterministic graph has a separate `specification.json` with safe/target sets and the manually checked robust-choice expectation. Implement a loader against these files now; no future exporter is required to obtain them. `python3 tools/check_fixture_packages.py` optionally checks the tiny fixture packages using only the standard library. CSV structural rules and cross-file rejection requirements are in the handoff document.

The [final structure](part1-final-structure.md) shows the future producer. Later obtain the integrated Part 1 implementation, rerun unchanged fixture checks, then substitute a validated real ModelBundle from `assembleModelBundle` for `fixtures::thermostat()` without changing consumer-facing APIs. A language-based consumer instead loads a future complete `generated_part1` package with the same schema. Separately supply safe/target/label/specification data, and return an `ISymbolicPolicy` using the exact action mapping and initial-memory convention from contracts.md. Joint Part 1/Part 2 tests become mandatory once both algorithms exist; successful fixture tests alone do not establish full-system correctness.
