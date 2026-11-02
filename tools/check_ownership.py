#!/usr/bin/env python3
"""Check committed, staged, unstaged and untracked paths, including both rename sides."""
import argparse
import os
import subprocess
import sys


def git(*args):
    return subprocess.check_output(["git", *args])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("owner", choices=["a", "b", "part2", "auto"])
    parser.add_argument("--base", default="part1-contract-v1")
    args = parser.parse_args()
    os.chdir(git("rev-parse", "--show-toplevel").decode().strip())
    owner = args.owner
    if owner == "auto":
        branch = git("branch", "--show-current").decode().strip()
        owner = next((o for prefix, o in [("part-a/", "a"), ("part-b/", "b"), ("part2/", "part2")]
                      if branch.startswith(prefix)), None)
        if owner is None:
            raise ValueError("no feature stream on this branch; use an explicit owner for an audit")
    directory = {"a": "part_a", "b": "part_b", "part2": "part2"}[owner]
    prefixes = tuple(f"{p}/{directory}/" for p in ["include/symctrl", "src", "tests", "docs"])
    # diff BASE includes both index and working-tree changes; -M catches renames.
    records = git("diff", "--name-status", "-z", "-M", args.base, "--").split(b"\0")
    paths = []
    i = 0
    while i < len(records) and records[i]:
        status = records[i].decode(); i += 1
        count = 2 if status.startswith(("R", "C")) else 1
        paths.extend(records[i:i + count]); i += count
    paths.extend(p for p in git("ls-files", "--others", "--exclude-standard", "-z").split(b"\0") if p)
    invalid = sorted({p.decode(errors="surrogateescape") for p in paths
                      if not p.decode(errors="surrogateescape").startswith(prefixes)})
    if invalid:
        raise ValueError("outside owner paths: " + ", ".join(repr(p) for p in invalid))
    print(f"OWNERSHIP OK {owner} against {args.base}: {len(set(paths))} changed paths")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print(f"OWNERSHIP FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
