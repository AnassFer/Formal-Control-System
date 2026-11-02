# Frozen baseline ownership

| Stream / branch | All allowed prefixes |
|---|---|
| A / `part-a/discretization` | `include/symctrl/part_a/`, `src/part_a/`, `tests/part_a/`, `docs/part_a/` |
| B / `part-b/abstraction` | `include/symctrl/part_b/`, `src/part_b/`, `tests/part_b/`, `docs/part_b/` |
| Later Part 2 / `part2/*` | `include/symctrl/part2/`, `src/part2/`, `tests/part2/`, `docs/part2/` |
| Shared coordinator | Every other tracked path is frozen at the common contract tag |

The root build already includes owner-local CMakeLists. Implement and extend tests inside your four trees; private headers stay in your include subtree. Public clients include `symctrl/contracts/*` only. All common helpers, fixtures, schema, root docs, CLI, integration/handoff tests, tools, CI and public factories are shared. Factory definitions are separated into A/B source trees. No cross-owner production links.

```sh
python3 tools/check_ownership.py auto
python3 tools/check_ownership.py a
python3 tools/check_ownership.py b
```

The checker compares the committed tree plus staged/unstaged changes to `part1-contract-v1`, and also checks nonignored untracked files. Rename detection checks both old and new paths. A shared-file rename into an allowed tree still fails. Run from anywhere in the checkout. Explicit owner selection on `main` is an audit, not authorisation to do feature work there. After a coordinated baseline update use, for example, `--base part1-contract-v1.1`; do not move v1 silently. No Git setting proves semantic compatibility; actual merged tests remain mandatory.
