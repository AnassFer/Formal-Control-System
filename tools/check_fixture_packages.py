#!/usr/bin/env python3
"""Test-only package validator. Never imports a model into production or exports one."""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def uint(text):
    require(re.fullmatch(r"0|[1-9][0-9]*", text) is not None, "noncanonical integer")
    value = int(text)
    require(value <= 2**64-1, "ID overflow")
    return value


def number(text):
    require(re.fullmatch(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", text) is not None,
            "invalid decimal")
    value = float(text)
    require(math.isfinite(value), "nonfinite CSV value")
    return value


def rows(path, header):
    raw = path.read_bytes()
    require(raw.endswith(b"\n") and b"\r" not in raw and not raw.startswith(b"\xef\xbb\xbf"), "CSV must use LF/no BOM")
    records = list(csv.reader(raw.decode("utf-8").splitlines(), strict=True))
    require(records and records[0] == header, "incorrect CSV header")
    require(all(len(r) == len(header) for r in records[1:]), "CSV row shape")
    return records[1:]


def validate(path):
    m = json.loads((path / "manifest.json").read_text())
    schema = json.loads((Path(__file__).resolve().parents[1] / "schemas/part1-model-v1.schema.json").read_text())
    require(set(m) == set(schema["required"]), "manifest fields differ from frozen v1")
    require(m["schema_version"] == m["contract_version"] == "1.0.0", "unsupported version")
    require(m["provenance"] == "hand_authored_fixture" and m["numeric_guarantee"] == "uncertified_float", "fixture provenance/guarantee")
    require(m["outside_state_id"] == 0, "OUT must be 0")
    n, p, q = (m["dimensions"][k] for k in ["state", "input", "disturbance"])
    M, A = m["internal_states"], m["input_count"]
    require(n > 0 and p > 0 and q >= 0 and M > 0 and A > 0, "invalid dimensions/counts")
    require(M < 2**64-1 and A <= 2**64-1, "count overflow")
    require(m["files"] == {"cells":"cells.csv", "inputs":"inputs.csv", "transitions":"transitions.csv"}, "filenames")
    for k, prop in schema["properties"]["semantics"]["properties"].items():
        require(m["semantics"].get(k) == prop.get("const") if "const" in prop else m["semantics"].get(k) in prop["enum"], "changed semantics")
    require(set(m["semantics"]) == set(schema["properties"]["semantics"]["required"]), "semantics fields")
    for box, dimension in [(m["grid"]["domain"], n), (m["disturbance"], q)]:
        require(set(box)=={"lo","hi"} and len(box["lo"])==len(box["hi"])==dimension,"box shape")
        require(all(math.isfinite(x) for v in box.values() for x in v), "nonfinite manifest coordinate")
        require(all(a<=b for a,b in zip(box["lo"],box["hi"])), "reversed box")
    cells = m["grid"]["cells_per_dimension"]
    require(len(cells) == n and all(type(c) is int and 0<c<2**32 for c in cells) and math.prod(cells)==M, "grid counts")
    require(all(a<b for a,b in zip(m["grid"]["domain"]["lo"],m["grid"]["domain"]["hi"])),"grid widths")
    cellrows=rows(path/"cells.csv", ["state_id"]+[f"lower_{d}" for d in range(n)]+[f"upper_{d}" for d in range(n)])
    require(len(cellrows)==M,"missing/extra cells")
    for i,r in enumerate(cellrows,1):
        require(uint(r[0])==i,"cell IDs must be ordered and complete")
        lo,hi=list(map(number,r[1:n+1])),list(map(number,r[n+1:]))
        require(all(a<b for a,b in zip(lo,hi)),"cell widths")
        # This validates tiny fixture geometry; it is not a production discretizer.
        offset=i-1
        for d in range(n):
            index=offset%cells[d]; offset//=cells[d]
            lower,upper=m["grid"]["domain"]["lo"][d],m["grid"]["domain"]["hi"][d]
            width=(upper-lower)/cells[d]
            require(math.isclose(lo[d],lower+index*width,rel_tol=0,abs_tol=1e-12)
                    and math.isclose(hi[d],lower+(index+1)*width,rel_tol=0,abs_tol=1e-12),"fixture geometry/ID mismatch")
    inputrows=rows(path/"inputs.csv",["input_id"]+[f"u_{d}" for d in range(p)])
    require(len(inputrows)==A,"missing/extra inputs")
    for i,r in enumerate(inputrows):
        require(uint(r[0])==i,"input IDs/order")
        for value in r[1:]:number(value)
    transitionrows=rows(path/"transitions.csv",["state_id","input_id","kind","successor_id"])
    pairs={};order=[]
    for s,a,kind,successor in transitionrows:
        s,a=uint(s),uint(a)
        require(s<=M and a<A,"invalid state/action ID")
        require(kind in {"explicit","all"},"invalid successor kind")
        require((kind=="all")== (successor==""),"universal/explicit row mismatch")
        sid=uint(successor) if successor else -1
        require(sid<=M,"invalid successor ID")
        pairs.setdefault((s,a),[]).append((kind,sid));order.append((s,a,sid))
    require(order==sorted(order) and len(order)==len(set(order)),"transition ordering/duplicates")
    require(set(pairs)=={(s,a) for s in range(M+1) for a in range(A)},"missing state/action pairs")
    for (state,_),entries in pairs.items():
        require(all(kind==entries[0][0] for kind,_ in entries),"mixed universal and explicit rows")
        require(entries[0][0]!="all" or len(entries)==1,"duplicate universal row")
        require(state!=0 or entries==[("all",-1)],"OUT must be universal")
    print(f"PACKAGE OK {path.name}: {M} internal cells, {A} inputs, {len(pairs)} complete pairs")


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packages",type=Path,nargs="*")
    args=parser.parse_args()
    try:
        for package in args.packages or [Path(__file__).resolve().parents[1]/"fixtures/part1_v1"/p for p in ["thermostat","nondeterministic_choice"]]:
            validate(package)
    except (ValueError,KeyError,TypeError,OSError,csv.Error) as error:
        print(f"PACKAGE INVALID: {error}",file=sys.stderr);sys.exit(1)
