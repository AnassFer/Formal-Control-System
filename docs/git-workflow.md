# Shared baseline and owner branches

The original tag part1-contract-v1 marks commit 25c6fe909cac531c2a3e1d2593cd598ff4522064. It is the initial scaffold, not a completed implementation. Main later received documentation. A/B branches originally started at the tag. This Markdown-only correction changes no refs, worktrees or history and performs no push.

```sh
git branch --show-current
git status --short
git worktree list
git rev-parse part1-contract-v1 part-a/discretization part-b/abstraction
```

## Local worktrees

An existing B worktree is /home/ilyazz/symctrl-b. Inspect/use it rather than recreating it. If an owner branch has no worktree and the destination is absent, setup commands from the original root are:

```sh
git worktree add ../symctrl-a part-a/discretization
git worktree add ../symctrl-b part-b/abstraction
```

Run only the setup actually needed. Each checkout has its own build directory. Do not switch branches inside another active session's folder or force a branch out of its current worktree.

## Sharing and different computers

The configured origin is https://github.com/AnassFer/Formal-Control-System. Local refs are not automatically published. Only after user authorisation and destination/history review, the coordinator can share main, A/B branches and the tag. A local Git bundle is another transfer method. Never force-push to overcome a history disagreement.

After authorised sharing, A can clone its branch:

```sh
git clone --branch part-a/discretization https://github.com/AnassFer/Formal-Control-System.git symctrl-a
```

B uses part-b/abstraction if cloning separately. Existing clones fetch available refs/tags before tracking the matching branch. Source archives supply files rather than Git history; ownership comparisons need the agreed Git baseline.

## Ownership and coordinated updates

Allowed prefixes are in [ownership.md](ownership.md). Owners change only their four trees, including local CMake/test/docs files. A/B operate through shared public views and remain independently testable.

```sh
python3 tools/check_ownership.py auto
python3 tools/check_readiness.py build/dev --mode a
```

B uses readiness mode b. Build prerequisites before readiness. Current gates cover the original scaffold, not all future synthesis/example requirements.

The coordinator makes a shared contract/build/test/documentation update once, validates it and records its comparison ref. Owners incorporate the same approved update before dependent work. Do not silently move the original tag. Use an explicit agreed --base ref for ownership after an updated baseline; do not invent a tag name before it exists. Main's unrelated documentation commits must not be merged into a feature branch solely to silence path errors.

## Later integration, not executed now

After both owners pass their required checks, fetch available current refs if using a remote. Create a clean integration worktree from the agreed baseline and normally merge the actual completed A/B refs, preserving their commits. For local refs and the original baseline, the shape is:

```sh
git worktree add -b integration/part1 ../symctrl-integration part1-contract-v1
cd ../symctrl-integration
git merge --no-ff part-a/discretization
git merge --no-ff part-b/abstraction
cmake -S . -B build/integration -DCMAKE_BUILD_TYPE=Release
cmake --build build/integration --parallel 2
ctest --test-dir build/integration --output-on-failure --no-tests=error
python3 tools/check_readiness.py build/integration --mode integration
```

Use origin/branch refs after fetching when those are the authoritative completed branches, or the updated baseline when documented. Resolve source/semantic conflicts without weakening shared assertions or hiding conflicts through ours strategies. Extend acceptance coverage as the general engine grows; the old registry is not full-paper completion.

Part 2 later branches from the available common baseline and supplies general synthesis in its reserved subtree. A subsequent robot example validates the reusable pipeline. Runtime/example command names must be implemented and documented through coordinated work rather than assumed to exist now. Review before moving integration to main under the user's workflow.
