#!/usr/bin/env python3
"""Optional developer gate; C++ build and ordinary CTest do not require Python."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description="Require present, enabled, passing tests with ZERO skips")
    parser.add_argument("build", type=Path)
    parser.add_argument("--mode", choices=["foundation", "a", "b", "integration"], default="integration")
    args = parser.parse_args()
    labels = {"foundation": {"contracts", "handoff"}, "a": {"contracts", "part_a"},
              "b": {"contracts", "handoff", "part_b"},
              "integration": {"contracts", "handoff", "part_a", "part_b", "integration"}}[args.mode]
    registry = json.loads((Path(__file__).parent / "required-tests.json").read_text())
    required = {name for label in labels for name in registry[label]}
    ctest = os.environ.get("CTEST", "ctest")
    listing = subprocess.run([ctest, "--test-dir", str(args.build), "--show-only=json-v1"],
                             check=True, text=True, capture_output=True)
    tests = json.loads(listing.stdout)["tests"]
    by_name = {t["name"]: t for t in tests}
    if len(by_name) != len(tests):
        raise ValueError("duplicate test names")
    missing = required - by_name.keys()
    if missing:
        raise ValueError("missing required tests: " + ", ".join(sorted(missing)))
    selected = set(required)
    for t in tests:
        props = {p["name"]: p["value"] for p in t.get("properties", [])}
        if labels.intersection(props.get("LABELS", [])):
            selected.add(t["name"])
        if t["name"] in selected and (props.get("DISABLED", False) or not t.get("command")):
            raise ValueError("disabled or unbuilt test: " + t["name"])
    if not selected:
        raise ValueError("zero tests cannot establish readiness")
    expression = "^(" + "|".join(re.escape(n) for n in sorted(selected)) + ")$"
    with tempfile.TemporaryDirectory(prefix="symctrl-readiness-") as tmp:
        report = Path(tmp) / "results.xml"
        result = subprocess.run([ctest, "--test-dir", str(args.build), "-R", expression,
                                 "--no-tests=error", "--output-on-failure", "--output-junit", str(report)])
        if not report.is_file():
            raise ValueError("CTest produced no results")
        cases = ET.parse(report).getroot().findall(".//testcase")
        actual = {c.attrib["name"] for c in cases}
        bad = [c.attrib["name"] for c in cases if c.find("skipped") is not None
               or c.find("failure") is not None or c.find("error") is not None
               or c.attrib.get("status", "run").lower() in {"notrun", "disabled"}]
        if result.returncode or actual != selected or len(cases) != len(selected) or bad:
            raise ValueError("readiness rejected (failed/skipped/missing results): " + ", ".join(bad))
    print(f"READY {args.mode}: {len(selected)} passed, zero skips")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.SubprocessError, ET.ParseError) as exc:
        print(f"NOT READY: {exc}", file=sys.stderr)
        sys.exit(1)
