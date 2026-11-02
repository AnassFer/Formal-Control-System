# Symbolic control: shared Part 1 foundation

This baseline prepares two students to implement Part 1 independently and gives a separate Part 2 group an immediately usable contract/fixture SDK. **All A/B production algorithms are throwing stubs. No Part 2 synthesis exists.** The CLI and future feature tests compile; only contracts, validation helpers and hand-authored fixture clients currently work.

A physical state has infinitely many possible real values. A future implementation partitions a bounded domain into finitely many cells, selects physical control vectors, and encloses every possible next state from each closed cell under every allowed disturbance. All intersecting cell closures become possible successors. Multiple successors represent nondeterminism, not probabilities. Simulating a centre alone cannot establish this enclosure.

The models are discrete in time and continuous in state: `x_next = f(x,u,w)`. No ODE integrator is needed. Abstraction maps a measured state to a cell ID; concretisation maps a symbolic action ID to its physical input vector. Later synthesis operates on this finite model and returns a policy.

Part 1 is Section 1 of paper S7467, through Section 1.2 (printed page 6 / PDF page 7). The slides' full safety-synthesis and largest-N competition is a later milestone. The A/B names are ownership labels, not article subsection names. See the [source map](docs/source-map.md).

## Build and actual bootstrap status

Prerequisites: C++17 compiler, CMake >=3.21 and Make (GCC/Clang on Linux). An ordinary configure/build/test uses only the standard library and requires no network or Python.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure --no-tests=error
ctest --test-dir build/dev -L 'contracts|handoff' --output-on-failure
ctest --test-dir build/dev -L part_a --output-on-failure
ctest --test-dir build/dev -L part_b --output-on-failure
ctest --test-dir build/dev -L integration --output-on-failure
./build/dev/examples/part2_fixture_consumer
```

Baseline result: **9 passed, 14 skipped, 0 failed** (23 registered tests). Five A cases, eight B cases and one production integration case skip only after encountering `NotImplemented`; they contain real future assertions. CTest's aggregate “100%” includes skips and does not establish feature readiness. [Validation evidence](docs/bootstrap-validation.md) records compiler/SDK/tool checks and limitations.

Optional Python 3 developer gates:

```sh
python3 tools/check_fixture_packages.py
python3 tools/check_readiness.py build/dev --mode foundation  # passes: 9, zero skips
python3 tools/check_readiness.py build/dev --mode a           # fails now: pending A
python3 tools/check_readiness.py build/dev --mode b           # fails now: pending B
python3 tools/check_readiness.py build/dev --mode integration # fails now: 14 skips
```

Readiness rejects missing, disabled, failed or skipped required tests, including zero-test builds. The required registry is `tools/required-tests.json`; additional owner tests with the relevant CTest label are included automatically. Do not weaken it to claim completion. `cmake --preset release` supplies Release, `sanitize` enables optional GCC/Clang address/undefined sanitizers, and `sdk` excludes A/B and the CLI. Use separate build directories per checkout.

## Ownership and handoff

| Owner | Allowed implementation paths |
|---|---|
| A | `include/symctrl/part_a/**`, `src/part_a/**`, `tests/part_a/**`, `docs/part_a/**` |
| B | `include/symctrl/part_b/**`, `src/part_b/**`, `tests/part_b/**`, `docs/part_b/**` |
| Part 2, later | The same four trees named `part2` |
| Frozen shared | All other tracked files, including contracts, integration tests, fixtures, CLI and root build/docs |

A owns grid discretisation, input catalogues and policy refinement. B owns dynamics, enclosures, graph construction/storage/export and abstraction statistics. B tests use tiny table-backed fake A views. Neither production library links the other or fixture support. Each owner has local CMake entry points. [Ownership rules](docs/ownership.md), [contribution instructions](CONTRIBUTING.md) and [Git/worktree/integration workflow](docs/git-workflow.md) give exact commands.

Public headers are in `include/symctrl/contracts/`. Read [exact API/conventions](docs/contracts.md), the [final Part 1 file blueprint](docs/part1-final-structure.md), [implementation status](docs/implementation-status.md), the [Part 1→2 contract/file format](docs/part1-to-part2.md) and [Part 2 quickstart](docs/part2-quickstart.md). The local baseline tag is `part1-contract-v1`; both requested branches start there: `part-a/discretization` and `part-b/abstraction`. No push is performed by bootstrap; local refs are not evidence that another student can fetch them.

## Thermostat and future CLI

The article's thermostat uses `x_next=(0.9-0.2*u)*x+0.1*w+6*u`, `X=[17,23]`, `W=[10,15]`, cells `[17,19)`, `[19,21)`, `[21,23]` and action IDs 0/1 for physical inputs OFF/ON. Cell 1 with OFF reaches `[16.3,18.6]`, so successors include OUT and cell 1. Cell 1 with ON reaches `[18.9,20.8]`, intersecting cells 1 and 2. The complete six-row oracle is frozen in [the source map](docs/source-map.md) and integration tests. A manually supplied policy chooses ON at state 1, OFF at 2/3; it is not synthesis output.

These commands are wired to production factories and **currently return exit code 2 with “not implemented”**:

```sh
./build/dev/apps/symctrl_cli thermostat
./build/dev/apps/symctrl_cli thermostat-policy
./build/dev/apps/symctrl_cli export-thermostat exports/thermostat
```

The future explicit benchmark is `./build/dev/apps/symctrl_cli building-n3`; it is documented here, not executed during bootstrap. It retains N=3, `[17,22]^3`, ten cells/axis and inputs `{0,0.3,0.6}^3`. Model configuration JSON in `examples/` is specification data, not an implemented config loader. Graph exports include IDs, ordering, closure/OUT semantics and truthful numerical status. Ordinary doubles are uncertified; no certified physical guarantee is claimed.

CI runs both compilers, the full bootstrap CTest suite and a separate SDK build. A manually selected strict readiness gate is available after implementation. Checkout v7 usage was verified against the [official action documentation](https://github.com/actions/checkout#usage); no hosted workflow has run as part of local bootstrap.
