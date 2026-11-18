# Part B validation record

Validated B implementation commit 2cb6630 against the current contract/schema 1.0.0 and comparison tag part1-docs-v2. Later owner-local status documentation does not change the tested code. Toolchain: CMake 4.4.4, GCC 15.2.0 and Clang 21.1.8 on Linux, C++17. No shared contract, schema, fixture, readiness registry or A implementation was changed.

## Results

| Check | Result |
|---|---|
| GCC dev configure/build | Pass |
| GCC full default CTest | 56 passed, six expected A/integration skips, zero failures |
| Strict B readiness | 56 passed, zero skips |
| Clang Release with address/undefined/leak sanitizers | 56 passed, six expected A/integration skips, zero findings |
| Ownership against part1-docs-v2 | Pass; only the four B trees changed |
| git diff --check | Pass |
| Generated package parsed with Python JSON/CSV and JSON Schema Draft 2020-12 | Pass |
| Combined A/B snapshot strict integration readiness | 62 passed, zero skips |

The 47 B cases comprise eight original regressions, nine monotone cases, eleven derivative cases, four dynamics-adapter cases, eight graph/bundle cases and seven export cases. Nine shared contract/handoff cases complete B's 56-case gate. Focused tests do not allow NotImplemented skips. The unchanged default suite still registers A tests even on B's independent branch; those six pending cases are outside the B gate.

## Reproduce B checks

From the B checkout, with CMake/CTest available:

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure --no-tests=error
python3 tools/check_readiness.py build/dev --mode b
python3 tools/check_ownership.py auto --base part1-docs-v2
git diff --check
```

The README lists selected targets for building just B and its foundation dependencies. On the current machine, a temporary CMake install is available under /tmp/symctrl-cmake/cmake/data/bin; prepend that directory to PATH when using these commands. Normal C++ builds/tests require no Python or network; the readiness/ownership and extra schema checks are developer tools.

Cross-compiler sanitizer configuration:

```sh
cmake -S . -B build/clang-sanitize -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DSYMCTRL_SANITIZERS=ON
cmake --build build/clang-sanitize --parallel 2
ctest --test-dir build/clang-sanitize --output-on-failure --no-tests=error
```

The managed execution sandbox traces processes, which prevents LeakSanitizer from running. The same instrumented tests passed outside that sandbox; no sanitizer was disabled.

## Independent package and A/B checks

A temporary client used public table-backed grid/input views, the real thermostat dynamics, bounded-derivative backend, graph builder, bundle assembly and exporter. Python parsed the generated manifest and CSV files, checked the frozen JSON schema, compared all CSV records against the independent thermostat fixture, and verified UTF-8/escaped metadata and a one-ULP numeric round trip. Production code remains independent of Python and fixture implementations.

The existing A branch at 8940efc and B implementation at 2cb6630 were combined using git merge-tree --write-tree. The conflict-free merged tree was 1e68e7ea1e4b488723d69e2fc0efb922189b49a9. An archive of this tree was built in a temporary directory with GCC Release, then checked using:

```sh
cmake -S . -B build/integration -DCMAKE_BUILD_TYPE=Release
cmake --build build/integration --parallel 2
python3 tools/check_readiness.py build/integration --mode integration
```

All 62 checks passed, including real grid/input/policy refinement and both B enclosure methods. This compatibility check changed no branch ref, tag or existing worktree and created no merge commit. Main and remote refs remain unchanged. The result covers the current acceptance suite, not an exhaustive audit of every possible A input.

## Completion boundary

The implemented B surface is complete for the agreed stages. All backends remain UncertifiedFloat. Model adapters require the analytic capabilities they advertise; the generic engine does not infer them from samples. Estimated graph-byte budgets cover persistent storage, not callback/transient process memory. Part 2 synthesis, validated interval arithmetic, periodic robot topology and large benchmarks remain outside this work.
