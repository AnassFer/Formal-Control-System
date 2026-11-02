# Local bootstrap validation record

Validated on Linux during initialisation, 2026-11-02. CMake 4.4.4, GCC 15.2.0 and Clang 21.1.8. CMake was initially absent; an approved one-time download installed it under `/tmp/symctrl-cmake`. No project dependency or network access was added to ordinary builds. This session used:

```sh
export PATH="/tmp/symctrl-cmake/cmake/data/bin:$PATH"
```

For later sessions, install/provide CMake >=3.21 normally; `/tmp` is transient. Python 3 is only optional tooling. JSON Schema validation below used the already available `jsonschema` developer module; the core C++ build/tests and stdlib fixture checker do not depend on it.

| Exact check | Actual result |
|---|---|
| `cmake --preset dev`; `cmake --build build/dev --parallel 4`; `ctest --test-dir build/dev --output-on-failure --no-tests=error` | GCC Debug: all targets compile; 9 passed, 14 skipped, 0 failed |
| `cmake -S . -B build/clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release`; build; CTest | Clang Release: same 9 passed / 14 pending / 0 failed; checks remain active with NDEBUG |
| `cmake --preset sdk`; build; `ctest --test-dir build/sdk --output-on-failure --no-tests=error` | No production targets; all 9 contracts/handoff tests pass, zero skips |
| `cmake -S . -B build/part2 -DSYMCTRL_BUILD_PRODUCTION=OFF -DSYMCTRL_BUILD_PART2=ON`; build; CTest | Reserved Part 2 subtree configures with no algorithms; the same 9 foundation/handoff tests pass |
| `./build/sdk/examples/part2_fixture_consumer` | Exit 0; exact output matches thermostat/expected-output.txt via handoff.consumer |
| `cmake -S examples/standalone -B build/external -DSYMCTRL_SDK_SOURCE="$PWD"`; build external_fixture_client; execute | Independent client exit 0; `nondeterministic_choice_fixture M=3 unsafe_successor_3=1` |
| Fresh build/a-isolated, fixtures OFF, targets contract_tests+part_a_tests | A/common compile without B/fixtures; 4 contract passes and 5 expected A skips |
| Fresh build/b-isolated, targets contract_tests+part_b_tests | B/common/fake fixtures compile without A; 4 contract passes and 8 expected B skips |
| SDK/external archive and executable-link inspection | No A/B archives built or linked by SDK/standalone/handoff clients; A/B test link dependencies are isolated |
| `cmake --preset sanitize -DCMAKE_CXX_COMPILER=clang++`; build contract_tests+part2_handoff_tests+part2_fixture_consumer; CTest labels contracts/handoff | Address/undefined sanitizer builds succeeded; 9 passed, zero skips outside the ptrace sandbox |
| `python3 tools/check_fixture_packages.py` | Both hand-authored packages pass; each has 3 regular cells, 2 inputs and all 8 state/action pairs |
| Draft202012Validator.check_schema plus validation of both manifests | Schema is well-formed and both concrete manifests validate; no schema library added to build requirements |
| `python3 tools/check_readiness.py build/dev --mode foundation` | Exit 0, READY: 9 passed with zero skips |
| Same readiness command with `--mode a`, `b`, `integration` | Each exits 1 as intended: required production tests are skipped |
| Temporary synthetic CTest registries: zero tests, missing required case, disabled required case, failed required case | Each is rejected by strict readiness (exit 1) |
| Temporary malformed fixture copies: missing state/action pair, duplicate row, input dimension mismatch | Each is rejected by test-only package validation (exit 1) |
| `symctrl_cli thermostat`, `thermostat-policy`, `export-thermostat exports/thermostat` | Each exits 2, `Part A uniform grid: not implemented`; no model files produced |
| Public include/link inspection | Shared headers include no private A/B header; no A→B→A cycle or hidden production fixture dependency |

Sanitizer detail: the initial sandbox run failed because LeakSanitizer explicitly reported that ptrace is unsupported. The identical tests were rerun with approved execution outside that sandbox and all nine passed. Leak checks were not disabled and assertions were not weakened.

The 14 pending tests are exactly five `part_a.*`, eight `part_b.*` and `integration.thermostat`. They return 77 only when the dedicated NotImplemented error is encountered. Ordinary CTest aggregates skip results; its “100%” line must not be reported as completed Part 1. Production A/B features and Part 2 algorithms remain wholly unimplemented. Fixtures do not prove universal enclosure soundness or certify a future physical controller.

No production building benchmark, integration feature merge or hosted CI run was performed. No remote push occurred. Source PDFs and the user's prompt stay local/uncommitted. The final Git audit reports the actual baseline hash, tag, both identical branch refs, main checkout and clean tracked tree. Later branch/worktree/sharing/integration commands are documentation only in git-workflow.md.
