# Local baseline and later collaboration

Default branch: `main` (initially unborn). Bootstrap creates one shared scaffold commit using the configured Git identity, lightweight tag `part1-contract-v1` and exactly these feature refs at that same commit: `part-a/discretization`, `part-b/abstraction`. It leaves the checkout on main, starts no feature work, creates no Part 2/integration feature branch and performs no push. Use `git rev-parse part1-contract-v1` for the exact commit; a commit does not embed its own hash in its files.

```sh
git branch --show-current
git status --short
git rev-parse part1-contract-v1 part-a/discretization part-b/abstraction
```

## Start the two owners locally

Run from the shared checkout after bootstrap:

```sh
git worktree add ../symctrl-a part-a/discretization
git worktree add ../symctrl-b part-b/abstraction
```

In separate terminals/sessions:

```sh
cd ../symctrl-a
cmake --preset dev
cmake --build build/dev --target part_a_tests --parallel 2
ctest --test-dir build/dev -L part_a --output-on-failure
```

```sh
cd ../symctrl-b
cmake --preset dev
cmake --build build/dev --target part_b_tests --parallel 2
ctest --test-dir build/dev -L part_b --output-on-failure
```

Each worktree's build/dev is physically separate. Do not switch a branch inside a checkout used by another session. A worktree is not another developer's remote share. Owners finish the full configure/build for contract prerequisites before running their strict readiness gate.

## Sharing and separate clones (future, not executed)

Existing origin is `https://github.com/AnassFer/Formal-Control-System`. Local creation does not establish remote visibility. Only after separate user authorisation and review of the destination/history, share the baseline and refs:

```sh
git push origin main part-a/discretization part-b/abstraction
git push origin part1-contract-v1
```

Never force-push to overcome history conflicts. If transferring to a different repository, use a clearly supplied URL, e.g. `git remote add shared USER_SUPPLIED_REMOTE_URL`, then explicitly choose that remote after authorisation. No remote is created/changed by bootstrap.

After authorised sharing, students can use separate clones:

```sh
git clone --branch part-a/discretization https://github.com/AnassFer/Formal-Control-System.git symctrl-a
git clone --branch part-b/abstraction https://github.com/AnassFer/Formal-Control-System.git symctrl-b
```

These clones track their matching remote branches; each has its own build tree. If remote sharing has not happened, local cloning from `'/home/ilyazz/Automate Project'` is also possible on this machine. Do not claim unpublished local refs can be fetched from GitHub.

## Ownership and shared changes

Before handoff, each owner builds all prerequisites and runs:

```sh
python3 tools/check_ownership.py auto
python3 tools/check_readiness.py build/dev --mode a
# B uses --mode b instead.
```

Readiness requires zero owner skips; B uses fake A fixtures and does not merge unfinished A for its isolated tests. Allowed paths are in `ownership.md`. The checker covers changes from the shared tag, staged/unstaged edits and nonignored untracked files, checking both rename paths. A proposed public change first becomes one coordinated common baseline update, with version, docs, schema/fixtures/tests validated. Both owners bring that same update into their branches before dependent work. Record the new comparison ref and use `--base NEW_SHARED_TAG`; never move v1 silently or hide conflicts with ours/merge attributes.

## Later integration (documentation only)

After A and B each pass readiness and ownership, fetch current remote refs if shared. In the coordinator's clean repository, inspect `git branch -a` and use the actual completed refs. For a shared remote, the full workflow is:

```sh
git fetch origin --tags
git worktree add -b integration/part1 ../symctrl-integration part1-contract-v1
cd ../symctrl-integration
git merge --no-ff origin/part-a/discretization
git merge --no-ff origin/part-b/abstraction
cmake -S . -B build/integration -DCMAKE_BUILD_TYPE=Release
cmake --build build/integration --parallel 2
ctest --test-dir build/integration --output-on-failure --no-tests=error
python3 tools/check_readiness.py build/integration --mode integration
./build/integration/apps/symctrl_cli thermostat
./build/integration/apps/symctrl_cli thermostat-policy
./build/integration/apps/symctrl_cli export-thermostat exports/thermostat
```

If development used local worktrees instead, merge the actual local `part-a/discretization` and `part-b/abstraction` refs in the two merge commands, after inspecting their tips. If a coordinated updated baseline exists, create integration from its documented tag. Normal merges preserve both owners' commits. Resolve real source/semantic conflicts; do not weaken shared assertions or suppress conflicts to make a merge appear clean.

Only after the small tests and strict gate pass, an optional explicit benchmark is `./build/integration/apps/symctrl_cli building-n3`. Report configuration, numeric status, edge/count/timing/storage limits and failures. Never start with larger-N allocation. External `/usr/bin/time -v` can measure process peak RSS where available; document that this includes setup/export if those are in the same invocation, while graph abstraction_seconds excludes export. Part 2's synthesis time/controllable percentage belongs to its later stage.

Review integration and move it to main only under the user's review workflow; bootstrap does neither. A clean textual merge is not mathematical/API compatibility. The strict zero-skip merged tests and real thermostat demonstration remain mandatory. Later Part 1/Part 2 substitution and joint algorithm checks are documented separately in `part2-quickstart.md`.
