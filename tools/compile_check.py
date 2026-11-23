#!/usr/bin/env python3
"""Compile every unit in game/units.txt with emscripten and report what fails.

    source ../fsadecomp/tools/emsdk/emsdk_env.sh
    python3 tools/compile_check.py [-j 16] [--filter SUBSTR] [--syntax-only]

Objects go to build/obj/<unit>.o, the full log to build/compile_report.txt.
The asset headers must exist first (tools/extract_assets.py main.dol build/assets).
"""
import argparse
import collections
import concurrent.futures as cf
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
BUILD = os.path.join(ROOT, "build")

# Flags every unit gets on top of its own include paths and defines.
# -nostdinc: the units use the decomp's own MSL headers for now.
PORT_FLAGS = [
    "-nostdinc",
    "-fno-rtti",
    "-fno-exceptions",
    "-fsigned-char",
    "-ffp-contract=off",
    "-DFSA_PORT=1",
    "-Wno-everything",
    "-ferror-limit=0",
    "-Wno-register",
]


def units():
    for line in open(os.path.join(GAME, "units.txt")):
        path, flags = line.rstrip("\n").split("\t")
        flags = flags.replace("@ASSETS@", os.path.join(BUILD, "assets"))
        # GNU modes: no trigraphs (the game spells resource types '????'), as MWCC.
        flags = flags.replace("--std=c++98", "--std=gnu++17").replace("--std=c99", "--std=gnu99").split()
        yield path, flags


def compile_one(path, flags, syntax_only, opt):
    obj = os.path.join(BUILD, "obj", path + ".o")
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    # -include port_prefix.h first: the compatibility layer for Metrowerks-isms.
    cmd = ["em++" if "c++" in flags else "emcc", *PORT_FLAGS, opt,
           "-include", os.path.join(GAME, "port", "port_prefix.h"), *flags]
    cmd += ["-fsyntax-only"] if syntax_only else ["-c", "-o", obj]
    cmd.append(path)
    p = subprocess.run(cmd, cwd=GAME, capture_output=True, text=True)
    errs = [l for l in p.stderr.splitlines() if ": error:" in l or "fatal error:" in l]
    if p.returncode and not errs:
        errs = [f"{path}: error: (no message) " + p.stderr.strip()[-300:]]
    return path, p.returncode == 0, errs, p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("--filter", default="")
    ap.add_argument("--syntax-only", action="store_true")
    ap.add_argument("-O", default="-O1")
    ap.add_argument("-v", action="store_true", help="print every error line")
    a = ap.parse_args()
    if not shutil.which("em++"):
        sys.exit("em++ not on PATH: source ../fsadecomp/tools/emsdk/emsdk_env.sh")
    todo = [(p, f) for p, f in units() if a.filter in p]
    with cf.ThreadPoolExecutor(a.j) as ex:
        res = list(ex.map(lambda u: compile_one(*u, a.syntax_only, a.O), todo))

    os.makedirs(BUILD, exist_ok=True)
    with open(os.path.join(BUILD, "compile_report.txt"), "w") as f:
        for path, ok, errs, log in res:
            if not ok:
                f.write(f"=== {path}\n{log}\n")

    groups = collections.Counter()
    by_file = collections.Counter()
    by_kind = collections.Counter()
    for path, ok, errs, _ in res:
        groups[path.split("/")[1] if path.startswith("src/game") else "JSystem"] += 0
        for e in errs:
            where = e.split(":")[0]
            by_file[where] += 1
            msg = e.split("error:", 1)[-1].strip()
            by_kind[re.sub(r"'[^']*'", "'X'", msg)[:100]] += 1
            if a.v:
                print(e)
    ok = sum(1 for r in res if r[1])
    print(f"{ok}/{len(res)} units compile")
    for kind, tree in (("game", "src/game/"), ("JSystem", ("libs/jsystem/", "src/JSystem/"))):
        sel = [r for r in res if r[0].startswith(tree)]
        print(f"  {kind}: {sum(1 for r in sel if r[1])}/{len(sel)}")
    print(f"{sum(by_file.values())} errors; top locations:")
    for k, v in by_file.most_common(15):
        print(f"  {v:6} {k}")
    print("top kinds:")
    for k, v in by_kind.most_common(12):
        print(f"  {v:6} {k}")


if __name__ == "__main__":
    main()
