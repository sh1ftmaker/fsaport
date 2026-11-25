#!/usr/bin/env python3
"""List what changed in fsa-decomp since the port last synced with it.

    python3 tools/decomp_sync.py [--decomp ../fsadecomp/fsa-decomp] [--to matching] [--out DIR]

The port is a hard fork of the decomp: game/ is the decomp's src/, include/ and
libs/ at the same paths, imported at cde64f65e and changed for the port since.
The decomp keeps improving those sources, mostly for matching (register
allocation, statement order, renames, inlining), sometimes for behaviour
(wrong resource tags, constants, offsets, missing branches, types). A sync
reads the decomp's diff since the last synced commit and ports the behaviour
changes by hand, keeping the port's adaptations (docs/PORTING.md, "Syncing
with the decomp").

The commit the port is synced to is in DECOMP_SYNC (first line:
`<branch> <full sha>`). Only the decomp's published branch, `matching`, is a
source (its commits passed the decomp's checks and review).

This script writes to --out (default build/decomp-sync/):
  files.tsv   every decomp file changed in the range: status, path, +/- lines,
              whether the port has the file, whether the port changed it since
              the import, and whether a 3-way merge of the decomp's change into
              the port's copy conflicts
  log.txt     the range's commits that touch src/, include/, libs/
  screen.txt  hunks whose string, character or numeric literals differ between
              the old and new side (tags, constants, offsets): a first pass, not
              the whole review: branches, argument order and types need reading
  merged/     the 3-way merge candidates (port + the decomp's whole change), for
              reference only; never copy them over the port's files
After the sync is committed, put the new commit in DECOMP_SYNC.
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMPORT_COMMIT = "d08ae8f"  # the port's import of the decomp tree (unmodified)


def git(repo, *a, check=True):
    return subprocess.run(["git", "-C", repo, *a], capture_output=True, check=check).stdout


LIT = re.compile(r"'(?:\\.|[^'\\])+'|\"(?:\\.|[^\"\\])*\"|\b0[xX][0-9a-fA-F]+\b|\b\d+\.\d*(?:[eE][-+]?\d+)?f?\b|\b\d+\b")


def literals(lines):
    out = []
    for l in lines:
        code = l.split("//")[0]
        # FSA_TAG64("Abcde") in the port is 'Abcde' in the decomp
        code = re.sub(r'FSA_TAG64\("([^"]*)"\)', r"'\1'", code)
        out += LIT.findall(code)
    return sorted(out)


def screen(diff_text):
    """Yield (header, hunk) for hunks whose literals differ."""
    cur_file, hunk, minus, plus = None, [], [], []

    def flush():
        if hunk and literals(minus) != literals(plus):
            return (cur_file, "".join(hunk))
        return None

    for line in diff_text.splitlines(keepends=True):
        if line.startswith("diff --git"):
            r = flush()
            if r:
                yield r
            cur_file, hunk, minus, plus = line.split(" b/")[-1].strip(), [], [], []
        elif line.startswith("@@"):
            r = flush()
            if r:
                yield r
            hunk, minus, plus = [line], [], []
        elif hunk:
            hunk.append(line)
            if line.startswith("-") and not line.startswith("---"):
                minus.append(line[1:])
            elif line.startswith("+") and not line.startswith("+++"):
                plus.append(line[1:])
    r = flush()
    if r:
        yield r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", default=os.path.join(ROOT, "..", "fsadecomp", "fsa-decomp"))
    ap.add_argument("--from", dest="frm", help="decomp commit to diff from (default: DECOMP_SYNC)")
    ap.add_argument("--to", default="matching", help="decomp ref to diff to (default: matching)")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "decomp-sync"))
    a = ap.parse_args()
    D = os.path.abspath(a.decomp)
    frm = a.frm or open(os.path.join(ROOT, "DECOMP_SYNC")).readline().split()[1]
    to = git(D, "rev-parse", a.to).decode().strip()
    os.makedirs(a.out, exist_ok=True)
    paths = ["src", "include", "libs"]
    print(f"decomp {frm[:10]}..{to[:10]} ({a.to})")

    open(os.path.join(a.out, "log.txt"), "wb").write(
        git(D, "log", "--no-merges", "--format=%h %ad %s", "--date=short", f"{frm}..{to}", "--", *paths))
    portmod = set(x[len("game/"):] for x in git(ROOT, "diff", "--name-only", IMPORT_COMMIT, "HEAD", "--", "game").decode().splitlines())
    rows = []
    for line in git(D, "diff", "--name-status", "-M", frm, to, "--", *paths).decode().splitlines():
        parts = line.split("\t")
        st, f = parts[0], parts[-1]
        pp = os.path.join(ROOT, "game", f)
        num = git(D, "diff", "--numstat", frm, to, "--", f).decode().split()
        conflicts = "-"
        if os.path.exists(pp) and st == "M":
            tmp = os.path.join(a.out, "merged", f)
            os.makedirs(os.path.dirname(tmp), exist_ok=True)
            open(tmp + ".base", "wb").write(git(D, "show", f"{frm}:{f}"))
            open(tmp + ".theirs", "wb").write(git(D, "show", f"{to}:{f}"))
            r = subprocess.run(["git", "merge-file", "-p", pp, tmp + ".base", tmp + ".theirs"], capture_output=True)
            open(tmp, "wb").write(r.stdout)
            conflicts = str(r.returncode)
        rows.append((st, f, f"+{num[0]}/-{num[1]}" if num else "", "port" if os.path.exists(pp) else "MISSING",
                     "port-modified" if f in portmod else "", conflicts))
    with open(os.path.join(a.out, "files.tsv"), "w") as fh:
        fh.write("status\tpath\tlines\tin_port\tport_modified\tmerge_conflicts\n")
        for r in rows:
            fh.write("\t".join(r) + "\n")
    n = 0
    with open(os.path.join(a.out, "screen.txt"), "w") as fh:
        diff = git(D, "diff", "-M", frm, to, "--", *paths).decode(errors="replace")
        for f, h in screen(diff):
            fh.write(f"=== {f}\n{h}")
            n += 1
    print(f"{len(rows)} files ({sum(r[3] == 'MISSING' for r in rows)} not in the port, "
          f"{sum(bool(r[4]) for r in rows)} changed by the port), {n} hunks with changed literals")
    print(f"written to {a.out}/ (files.tsv, log.txt, screen.txt, merged/)")


if __name__ == "__main__":
    sys.exit(main())
