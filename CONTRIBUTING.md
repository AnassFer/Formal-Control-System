# Development after bootstrap

Start from `part1-contract-v1` on the matching feature branch. Read `AGENTS.md`, `docs/contracts.md` and your owner notes before implementation. Do not develop features on `main` or change frozen paths on a feature branch. Keep A and B independently buildable; fixtures are test-only.

Add implementation sources and tests using your own `src/part_a/CMakeLists.txt` or `src/part_b/CMakeLists.txt` and matching test CMakeLists. No globs or shared-root edits are needed. Give added tests the `part_a`/`part_b` label. Explicit checks must survive Release builds; only `NotImplemented` can yield feature skip code 77.

Before handing off, run the build, your labelled tests, `python3 tools/check_readiness.py build/dev --mode a` (or `b`) and `python3 tools/check_ownership.py auto`. Zero skipped owner tests are required for readiness. Keep notes under your owner-specific docs directory. Do not modify shared assertions, substitute fixture output for algorithms or inflate numerical guarantee metadata.

For a contract gap, document the failing use case, proposed exact signature/semantic/serialization change, version impact and affected tests. Coordinate one common update and documented comparison tag before both branches continue. Later integration uses normal merges and the strict integration gate described in `docs/git-workflow.md`; a clean textual merge is insufficient.

Keep build artefacts, exported models, timing reports and local source PDFs out of commits. Preserve unrelated work and never force-push, reset or clean it. Remote sharing needs separate user authorisation.
