# General Part 2 quickstart

This is a starting base for later model-independent controller synthesis. Read [PART2_HANDOFF.md](../PART2_HANDOFF.md). Part 1 supplies a model through shared views; Part 2 receives a separate specification and returns a policy. The later robot example follows the reusable pipeline. No synthesis algorithm exists yet.

## Obtain and build the base

After authorised sharing, clone the common tag from an available local/shared repository. On the original machine a local example is:

```sh
git clone --branch part1-contract-v1 '/home/ilyazz/Automate Project' symctrl-sdk
cd symctrl-sdk
cmake --preset sdk
cmake --build build/sdk --parallel 2
ctest --test-dir build/sdk --output-on-failure --no-tests=error
./build/sdk/examples/part2_fixture_consumer
```

On another machine replace the clone source with the repository/bundle actually shared with you. Nothing has been pushed by this documentation task. The tag contains the original scaffold; current explanatory Markdown may be shared separately until a coordinator update is committed. A source archive contains files but no branch/tag history.

The nine existing contract/handoff cases pass without A/B production targets. The consumer queries fixed fixture data and performs no synthesis. This establishes a usable boundary, not completed algorithms.

## Same-repository workspace

Once the agreed baseline is available and your current worktree is clean, a group can later create its branch:

```sh
git switch -c part2/controller part1-contract-v1
cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON
cmake --build build/part2 --parallel 2
ctest --test-dir build/part2 --output-on-failure --no-tests=error
```

Use only include/symctrl/part2/, src/part2/, tests/part2/ and docs/part2/. The root already includes local CMake entry points behind SYMCTRL_BUILD_PART2. symctrl_part2_workspace is an INTERFACE connection point; create a normal compiled target as sources are added. Keep new algorithms/tests out of the shared consumer and fixed handoff tests.

With a valid baseline ref, `python3 tools/check_ownership.py part2` checks path ownership. There is no Part 2 algorithm-readiness suite yet; define meaningful algorithm tests when implementation is authorised. Public specification/result/factory details need coordinated agreement first.

## Separate C++ project

The SDK consists of public contracts, minimal common helpers, fixture support/data, schema, build support and examples. Headers declare operations; link symctrl_common for the completed helpers and symctrl_fixture_support for fixed test models. Private producer classes are not needed.

The standalone client is tested through add_subdirectory:

```sh
cmake -S examples/standalone -B build/external -DSYMCTRL_SDK_SOURCE="$PWD"
cmake --build build/external --target external_fixture_client --parallel 2
./build/external/external_fixture_client
```

Copy that small client project to your own repository and point SYMCTRL_SDK_SOURCE at the shared checkout. It builds no A/B library. symctrl_contracts is an INTERFACE declaration target, symctrl_common supplies helpers, and fixtures remain a test/example dependency.

## File route and future integration

Study fixtures/part1_v1/nondeterministic_choice/ and schemas/part1-model-v1.schema.json. The fixed graph is a general nondeterministic example; specification.json separately supplies safe/target sets and a manually checked robust action. No unimplemented exporter is needed to obtain the sample files.

[part1-to-part2.md](part1-to-part2.md) specifies IDs, universal/empty semantics, CSV ordering/count validation and versions. Later load a real generic package or replace the fixture bundle with a real ModelBundle. The policy boundary stays ISymbolicPolicy. Rerun unchanged fixture checks and add joint synthesis/refinement tests. Fixed-fixture success alone cannot prove a completed general or physical controller.
