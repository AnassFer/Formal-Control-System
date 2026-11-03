# Historical scaffold validation record

This is an abridged record of the original 2 November 2026 scaffold checks, not validation of the later general implementation plan. This documentation-only revision does not rerun or change production code/tests. See [the scope note](scope-correction.md).

The original environment used Linux, CMake 4.4.4, GCC 15.2.0 and Clang 21.1.8. CMake was initially absent and an approved one-time download installed it under /tmp/symctrl-cmake. Ordinary builds gained no network dependency. That temporary installation is not permanent.

| Check originally run | Result |
|---|---|
| GCC Debug full scaffold configure/build/CTest | All targets compiled; 9 passes, 14 expected feature skips, 0 failures |
| Clang Release full scaffold | Same test counts; explicit checks remained active |
| SDK-only build/CTest | Nine passes, zero skips, no A/B libraries |
| Fixture consumer and output comparison | Successful fixed-data client |
| Standalone add_subdirectory consumer | Successful independent client, no A/B libraries |
| Isolated A target build | A/common without B/fixtures; four contract passes and five expected A skips |
| Isolated B target build | B/common/fake views without A; four contract passes and eight expected B skips |
| Address/undefined sanitizer foundation/handoff checks | Nine passes outside ptrace sandbox |
| Manifest schema and fixed package validation | Both original samples passed |
| Foundation readiness | Nine passes, zero skips |
| A/B/integration readiness | Correctly rejected pending production cases |
| Synthetic missing/disabled/failed/zero-test registries | Correctly rejected |
| Malformed fixed package copies | Missing pair, duplicate row and dimension mismatch rejected |
| Production CLI skeleton checks | Missing implementation reported with nonzero status |
| Include/link/ownership audit | Expected isolation; original refs agreed and worktree was clean after bootstrap |

The first sanitizer run reported LeakSanitizer's ptrace restriction. Approved execution of identical checks outside that sandbox passed without disabling leak checks. Optional JSON Schema validation used an already available developer module; core C++ checks do not depend on it.

The 14 skipped cases were exactly five A, eight B and one integration. Only NotImplemented triggered those skips. The evidence demonstrates usable contracts/fixtures and a compiling scaffold. It does not demonstrate production graph generation, synthesis, robot completion or certified physical guarantees.

The source PDFs remained local/uncommitted. No production benchmark, feature merge, hosted CI execution or remote push was performed by the original bootstrap. Later generic acceptance must be added and reported from actual implementations.
