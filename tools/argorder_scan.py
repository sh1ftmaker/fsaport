#!/usr/bin/env python3
"""List argument lists whose evaluation order matters (MWCC: right to left).

MWCC evaluates function and constructor arguments right to left; clang
evaluates them left to right. Where two or more arguments of one call have
side effects that interact (RNG draws above all), the port gets a different
result from the original. This scans every game unit with libclang (the
build's own flags from build/web/compile_commands.json) and lists each call,
constructor call, operator call and braced initializer list in game/ with
at least two arguments that have side effects.

Pass 1 (per unit, in parallel): every function definition under game/ with
what it does directly (writes to non-locals, calls, calls it cannot resolve)
and every call site with, per argument, the callees and direct side effects.
Pass 2 (global): which functions are impure and which draw from the RNG
(JMath::TRandom_fast_, transitively), then each site's class:

  rng       >= 2 arguments draw from the RNG: the order is definitely wrong
  rng+eff   one argument draws, another has other side effects (review)
  effect    >= 2 arguments have side effects, none draws (review)
  obj       the object expression of a member call and an argument both
            have side effects (review)

Const methods count as pure; functions without a definition in game/ that
are not const count as impure (conservative). Output: TSV on stdout or -o.

    python3 tools/argorder_scan.py -p build/web -o logs/argorder/scan.tsv
"""
import argparse
import json
import multiprocessing
import os
import re
import shlex
import sys

from clang import cindex
from clang.cindex import CursorKind as K

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game") + os.sep

# sequenced (&&, ||, comma) or not a value computation of both sides
UNORDERED_EXEMPT = {cindex.BinaryOperator.LAnd, cindex.BinaryOperator.LOr, cindex.BinaryOperator.Comma}
FUNC_KINDS = {K.FUNCTION_DECL, K.CXX_METHOD, K.CONSTRUCTOR, K.DESTRUCTOR,
              K.CONVERSION_FUNCTION, K.FUNCTION_TEMPLATE}
# classes whose non-const methods draw random numbers
RNG_CLASSES = {"TRandom_fast_", "TRandom_", "TRandom_enough_", "stRandom", "JPARandom", "Random"}
# classes whose methods count as pure (the port's endian wrappers)
PURE_CLASSES = {"BE", "BEPtr", "BEOffset"}


def in_game(cur):
    f = cur.location.file
    return f is not None and f.name.startswith(GAME)


def rel(path):
    return os.path.relpath(path, ROOT)


def unary_incdec(cur):
    toks = [t.spelling for t in cur.get_tokens()]
    return bool(toks) and (toks[0] in ("++", "--") or toks[-1] in ("++", "--"))


def target_of(cur, func):
    """What a write or a member call acts on: 'local' (a local variable or
    by-value parameter of func, or a member of one), 'this' (a member of
    *this; a constructor's or destructor's own object counts as 'local'),
    or 'other'."""
    c = strip(cur)
    if c is None:
        return "other"
    if c.kind == K.DECL_REF_EXPR:
        d = c.referenced
        if d is not None and d.kind == K.PARM_DECL:
            if d.type.kind in (cindex.TypeKind.LVALUEREFERENCE, cindex.TypeKind.RVALUEREFERENCE):
                return "other"
            return "local"
        if d is not None and d.kind == K.VAR_DECL:
            p = d.semantic_parent
            if p is not None and p.kind in FUNC_KINDS and d.storage_class != cindex.StorageClass.STATIC \
                    and d.type.kind not in (cindex.TypeKind.LVALUEREFERENCE, cindex.TypeKind.RVALUEREFERENCE):
                return "local"
        return "other"
    if c.kind == K.CXX_THIS_EXPR:
        if func is not None and func.kind in (K.CONSTRUCTOR, K.DESTRUCTOR):
            return "local"
        return "this"
    if c.kind == K.MEMBER_REF_EXPR:
        ch = list(c.get_children())
        if not ch:  # implicit this
            if func is not None and func.kind in (K.CONSTRUCTOR, K.DESTRUCTOR):
                return "local"
            return "this"
        base = strip(ch[0])
        if base.kind == K.CXX_THIS_EXPR:
            return target_of(base, func)
        if base.type.kind == cindex.TypeKind.POINTER:
            return "other"
        return target_of(base, func)
    if c.kind == K.ARRAY_SUBSCRIPT_EXPR:
        ch = list(c.get_children())
        if ch and strip(ch[0]).type.kind == cindex.TypeKind.CONSTANTARRAY:
            return target_of(ch[0], func)
    return "other"


class Facts:
    """What a subtree does: calls ("usr|target"), unresolved calls, writes
    (their targets)."""
    __slots__ = ("calls", "unknown", "writes", "names")

    def __init__(self):
        self.calls = set()
        self.unknown = False
        self.writes = []
        self.names = set()


def has_body(d):
    df = d.get_definition()
    # an implicit member's extent is just the class name
    return df is not None and any(ch.kind == K.COMPOUND_STMT for ch in df.get_children()) \
        and len(list(df.get_tokens())) > 1


def callee_of(cur):
    d = cur.referenced
    if d is None:
        return None
    if d.kind == K.FUNCTION_TEMPLATE or d.kind in FUNC_KINDS:
        return d
    return None


def call_target(c, d, func):
    if d.kind == K.CONSTRUCTOR:
        return "new"
    if d.kind not in (K.CXX_METHOD, K.CONVERSION_FUNCTION, K.DESTRUCTOR) or d.is_static_method():
        return "free"
    first = next(c.get_children(), None)
    fs = strip(first)
    if fs is not None and fs.kind == K.MEMBER_REF_EXPR:
        ch = list(fs.get_children())
        if not ch:
            return target_of(fs, func) if False else ("local" if func is not None and func.kind in (K.CONSTRUCTOR, K.DESTRUCTOR) else "this")
        base = strip(ch[0])
        if base.kind != K.CXX_THIS_EXPR and base.type.kind == cindex.TypeKind.POINTER:
            return "other"
        return target_of(base, func)
    # operator call: the object is the first argument
    args = list(c.get_arguments())
    if args:
        a0 = strip(args[0])
        if a0 is not None and a0.kind == K.UNARY_OPERATOR:  # *p
            return "other"
        return target_of(args[0], func)
    return "other"


def is_const(d):
    return d.kind in (K.CXX_METHOD, K.CONVERSION_FUNCTION) and d.is_const_method()


def in_template(func):
    c = func
    while c is not None and c.kind != K.TRANSLATION_UNIT:
        if c.kind in (K.FUNCTION_TEMPLATE, K.CLASS_TEMPLATE, K.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION):
            return True
        c = c.semantic_parent
    return False


def usr_key(d):
    """The USR of the declaration whose body we record: a template
    instantiation's pattern."""
    t = d.specialized_template
    if t is not None and t.kind in FUNC_KINDS:
        return t.get_usr()
    return d.get_usr()


def walk_facts(cur, facts, func):
    """Collect facts over cur's subtree (not crossing lambdas). Calls a
    template body cannot resolve (dependent) are not counted."""
    tmpl = func is not None and in_template(func)
    stack = [cur]
    while stack:
        c = stack.pop()
        k = c.kind
        if k == K.LAMBDA_EXPR:
            continue
        if k == K.CALL_EXPR:
            d = callee_of(c)
            if d is None:
                # function pointer / dependent call; empty spelling = a cast
                if c.spelling and not c.spelling.startswith("__builtin") and not tmpl:
                    facts.unknown = True
                    facts.names.add("?" + c.spelling)
            elif d.kind == K.CONSTRUCTOR and not has_body(d) and (
                    d.is_copy_constructor() or d.is_move_constructor() or d.is_default_constructor()):
                pass  # implicit copy/move/default constructor: pure
            elif d.kind == K.CXX_METHOD and not has_body(d) and (
                    d.is_copy_assignment_operator_method() or d.is_move_assignment_operator_method()):
                facts.writes.append(call_target(c, d, func))  # implicit operator=: a plain write
            elif is_const(d) or (d.semantic_parent is not None and d.semantic_parent.spelling in PURE_CLASSES):
                pass  # const methods count as pure
            else:
                t = call_target(c, d, func)
                facts.calls.add(usr_key(d) + "|" + t)
                facts.names.add(d.spelling)
                if d.kind == K.CXX_METHOD and d.is_virtual_method():
                    facts.calls.add("virtual:" + usr_key(d) + "|" + t)
        elif k == K.BINARY_OPERATOR:
            if c.binary_operator.is_assignment:
                lhs = next(c.get_children(), None)
                facts.writes.append(target_of(lhs, func) if lhs is not None else "other")
        elif k == K.COMPOUND_ASSIGNMENT_OPERATOR:
            lhs = next(c.get_children(), None)
            facts.writes.append(target_of(lhs, func) if lhs is not None else "other")
        elif k == K.UNARY_OPERATOR:
            if unary_incdec(c):
                ch = next(c.get_children(), None)
                facts.writes.append(target_of(ch, func) if ch is not None else "other")
        elif k == K.CXX_NEW_EXPR or k == K.CXX_DELETE_EXPR:
            facts.writes.append("other")
        stack.extend(c.get_children())
    return facts


def strip(c):
    while c is not None and c.kind in (K.UNEXPOSED_EXPR, K.PAREN_EXPR):
        ch = list(c.get_children())
        if len(ch) != 1:
            break
        c = ch[0]
    return c


def src_text(cur, limit=160):
    try:
        s = " ".join(t.spelling for t in cur.get_tokens())
    except Exception:
        s = ""
    return s if len(s) <= limit else s[:limit] + "..."


def facts_json(f):
    return {"calls": sorted(f.calls), "unknown": f.unknown, "writes": f.writes,
            "names": sorted(f.names)}


def scan_unit(entry):
    args = shlex.split(entry["command"])
    flags = []
    skip = False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-c"):
            skip = a == "-o"
            continue
        if a == entry["file"]:
            continue
        flags.append(a)
    flags = ["-target", "wasm32-unknown-emscripten", "-D__EMSCRIPTEN__"] + flags
    idx = cindex.Index.create()
    try:
        tu = idx.parse(entry["file"], args=flags,
                       options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES * 0)
    except cindex.TranslationUnitLoadError as e:
        return {"file": entry["file"], "error": str(e)}
    errors = [str(d) for d in tu.diagnostics if d.severity >= cindex.Diagnostic.Error]
    funcs = {}
    sites = []

    def visit(cur, func):
        for c in cur.get_children():
            if not in_game(c):
                continue
            k = c.kind
            if k in FUNC_KINDS and c.is_definition():
                usr = c.get_usr()
                if usr not in funcs:
                    f = walk_facts_body(c)
                    funcs[usr] = {"name": c.spelling, "loc": "%s:%d" % (rel(c.location.file.name), c.location.line),
                                  "const": is_const(c),
                                  "rng": is_rng_method(c), **facts_json(f)}
                visit(c, c)
                continue
            if k in (K.CALL_EXPR, K.INIT_LIST_EXPR):
                record_site(c, func)
            elif (k == K.BINARY_OPERATOR and c.binary_operator not in UNORDERED_EXEMPT) \
                    or k == K.COMPOUND_ASSIGNMENT_OPERATOR:
                record_site(c, func)
            visit(c, func)

    def walk_facts_body(fn):
        f = Facts()
        for ch in fn.get_children():
            if ch.kind == K.COMPOUND_STMT or ch.kind == K.MEMBER_REF or ch.kind == K.CALL_EXPR \
                    or ch.kind == K.UNEXPOSED_EXPR or ch.kind == K.INIT_LIST_EXPR:
                walk_facts(ch, f, fn)
        return f

    def is_rng_method(fn):
        p = fn.semantic_parent
        return p is not None and p.spelling in RNG_CLASSES and not is_const(fn)

    def record_site(c, func):
        if c.kind == K.CALL_EXPR:
            argl = [a for a in c.get_arguments()]
            obj = None
            first = next(c.get_children(), None)
            if first is not None:
                fs = strip(first)
                if fs is not None and fs.kind == K.MEMBER_REF_EXPR:
                    base = next(fs.get_children(), None)
                    if base is not None and base.kind != K.CXX_THIS_EXPR:
                        obj = base
            d = callee_of(c)
            name = d.spelling if d is not None else c.spelling
            kind = "call"
            if d is not None and d.kind == K.CONSTRUCTOR:
                kind = "ctor"
            elif name.startswith("operator"):
                kind = "op"
        elif c.kind == K.INIT_LIST_EXPR:
            argl = list(c.get_children())
            obj = None
            name = "{...}"
            kind = "init"
        else:
            argl = list(c.get_children())
            obj = None
            name = c.binary_operator.name if c.kind == K.BINARY_OPERATOR else "op="
            kind = "binop"
        argl = [a for a in argl if a is not None]
        if len(argl) + (obj is not None) < 2:
            return
        af = []
        for a in argl:
            f = walk_facts(a, Facts(), func)
            af.append(facts_json(f))
        of = facts_json(walk_facts(obj, Facts(), func)) if obj is not None else None
        # cheap pre-filter: at least two parts call something or write
        parts = af + ([of] if of else [])
        live = sum(1 for p in parts if p["calls"] or p["unknown"] or p["writes"])
        if live < 2:
            return
        sites.append({"loc": "%s:%d:%d" % (rel(c.location.file.name), c.location.line, c.location.column),
                      "func": func.spelling if func is not None else "",
                      "callee": name, "kind": kind, "args": af, "obj": of,
                      "argtext": [src_text(a) for a in argl],
                      "objtext": src_text(obj) if obj is not None else None})

    visit(tu.cursor, None)
    return {"file": entry["file"], "errors": errors[:5], "funcs": funcs, "sites": sites}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", default=os.path.join(ROOT, "build/web"), help="build dir with compile_commands.json")
    ap.add_argument("-o", default=None, help="TSV output (default stdout)")
    ap.add_argument("-j", type=int, default=max(1, os.cpu_count() // 2))
    ap.add_argument("--json", default=None, help="also write the raw pass-1 facts here")
    ap.add_argument("--filter", default=None, help="regex of units to scan")
    a = ap.parse_args()
    cc = json.load(open(os.path.join(a.p, "compile_commands.json")))
    units = [e for e in cc if e["file"].startswith(os.path.join(ROOT, "game/src") + os.sep)
             or e["file"].startswith(os.path.join(ROOT, "game/libs") + os.sep)]
    if a.filter:
        units = [e for e in units if re.search(a.filter, e["file"])]
    with multiprocessing.Pool(a.j) as pool:
        results = pool.map(scan_unit, units, chunksize=1)

    funcs = {}
    sites = {}
    for r in results:
        if r.get("error") or r.get("errors"):
            print("warning: %s: %s" % (rel(r["file"]), r.get("error") or r["errors"][0]), file=sys.stderr)
        for u, f in r.get("funcs", {}).items():
            funcs.setdefault(u, f)
        for s in r.get("sites", []):
            sites.setdefault(s["loc"], s)
    if a.json:
        json.dump({"funcs": funcs, "sites": list(sites.values())}, open(a.json, "w"))

    # pass 2: E (effects outside the function's own object and locals),
    # T (writes its own object, *this) and rng (draws), to a fixpoint
    E, T, R = {}, {}, {}
    for u, f in funcs.items():
        E[u] = ("other" in f["writes"] or f["unknown"]) and not f["const"]
        T[u] = "this" in f["writes"] and not f["const"]
        R[u] = f["rng"]

    def split(c):
        usr, _, t = c.rpartition("|")
        return usr, t

    def eff_of(usr):
        if usr.startswith("virtual:"):
            return True, True, False
        if usr not in funcs:
            return not undefined_pure(usr), False, False
        return E[usr], T[usr], R[usr]

    changed = True
    while changed:
        changed = False
        for u, f in funcs.items():
            if f["const"]:
                continue
            for c in f["calls"]:
                usr, t = split(c)
                e, tw, r = eff_of(usr)
                if r and not R[u]:
                    R[u] = changed = True
                if (e or (tw and t == "other")) and not E[u]:
                    E[u] = changed = True
                if tw and t == "this" and not T[u]:
                    T[u] = changed = True

    def part(p):
        if p is None:
            return None
        is_rng = False
        why = []
        if p["writes"]:
            why.append("write")
        if p["unknown"]:
            why.append("indirect call")
        for c in p["calls"]:
            usr, t = split(c)
            e, tw, r = eff_of(usr)
            is_rng |= r
            if e or r or (tw and t not in ("new", "free")):
                why.append(usr_name(usr) + ("@" + t if t not in ("new", "free") else "") + ("[rng]" if r else ""))
        return is_rng, bool(why), sorted(set(why))

    def usr_name(usr):
        f = funcs.get(usr.replace("virtual:", ""))
        n = f["name"] if f else re.sub(r"#.*$", "", usr).split("@")[-1]
        return ("virtual " if usr.startswith("virtual:") else "") + n

    rows = []
    for loc, s in sorted(sites.items(), key=lambda kv: sort_key(kv[0])):
        parts = [part(p) for p in s["args"]]
        nrng = sum(1 for p in parts if p[0])
        neff = sum(1 for p in parts if p[1])
        o = part(s["obj"])
        cls = None
        if nrng >= 2:
            cls = "rng"
        elif nrng == 1 and neff >= 2:
            cls = "rng+eff"
        elif neff >= 2:
            cls = "effect"
        elif o is not None and o[1] and neff >= 1:
            cls = "obj"
        if cls is None or (s["kind"] == "binop" and cls not in ("rng", "rng+eff")):
            continue
        which = "; ".join("[%d] %s" % (i, " ".join(p[2])) for i, p in enumerate(parts) if p[1])
        if cls == "obj":
            which += "; obj " + " ".join(o[2])
        rows.append((loc, cls, s["func"], s["kind"], s["callee"], which,
                     " | ".join(("[%d] " % i) + t for i, t in enumerate(s["argtext"]))
                     + (("  obj: " + s["objtext"]) if cls == "obj" else "")))
    out = open(a.o, "w") if a.o else sys.stdout
    out.write("loc\tclass\tfunction\tkind\tcallee\teffects\targs\n")
    for r in rows:
        out.write("\t".join(x.replace("\t", " ") for x in r) + "\n")
    print("%d units, %d functions, %d candidate sites, %d listed" % (len(units), len(funcs), len(sites), len(rows)),
          file=sys.stderr)


PURE_UNDEFINED = re.compile(r"^c:@F@(sqrt|sqrtf|fabs|fabsf|sin|sinf|cos|cosf|tan|tanf|atan|atanf|atan2|atan2f|"
                            r"floor|floorf|ceil|ceilf|fmod|fmodf|pow|powf|abs|labs|acos|acosf|asin|asinf|"
                            r"strlen|strcmp|strncmp|memcmp|__fabs|__fabsf|__frsqrte|__fnabs)#")


def undefined_pure(usr):
    return bool(PURE_UNDEFINED.match(usr))


def sort_key(loc):
    f, l, c = loc.rsplit(":", 2)
    return (f, int(l), int(c))


if __name__ == "__main__":
    main()
