#!/usr/bin/env python3
"""Which case and branch of fn_80383EB0's command switch each msg_eval poke took.

  msg_cases.py SCAN_RUN/harness_msg.jsonl EVAL_RUN/harness_poke.jsonl

The scan gives each conditional message's command bytes (0x1A, size, 2, 0,
command, operands...); the eval run gives fn_80383EB0(window, index) -> result.
The branch is the operand the result equals (offsets as fn_80383EB0 reads
them). A result that is none of the first command's operands went through a
further conditional message (a chain): the chain is followed through the scan
when the step it took can be told from the result.
"""
import collections, json, sys

OPS = {  # command: [(operand offset, branch name)]
    0x07: [(6, 'screen flag set'), (10, 'screen flag clear')],
    0x10: [(6, 'holds the item (target)'), (10, 'default (next)')],
    0x18: [(5, '1 player'), (9, '2 players'), (13, '3 players'), (17, '4 players')],
    0x1D: [(9, 'fn_80236100 >= limit (high)'), (13, 'below limit')],
    0x20: [(5, 'slot 0'), (9, 'slot 1'), (13, 'slot 2'), (17, 'slot 3')],
    0x21: [(5, 'stage flag set'), (9, 'stage flag clear')],
    0x23: [(8, 'view bit set'), (12, 'view bit clear')],
    0x2C: [(5, 'field_0x83 kind == 2'), (9, 'kind != 2')],
    0x30: [(5, 'fn_8022CAF8 != 4'), (9, 'fn_8022CAF8 == 4')],
    0x33: [(5, 'fn_802373EC == 1'), (9, 'fn_802373EC != 1')],
}
msgs = {}
for line in open(sys.argv[1]):
    o = json.loads(line)
    msgs[o['index']] = (o['command'], bytes.fromhex(o['bytes']))


def ops(cmd, b):
    out = []
    for off, name in OPS.get(cmd, []):
        if off + 4 <= len(b):
            out.append((int.from_bytes(b[off:off + 4], 'big'), name))
    return out


def reach(i, target, depth=0):
    """The first-step branch names of message i that can lead to target."""
    if i == target and i not in msgs:
        return True
    if depth > 6 or i not in msgs:
        return i == target
    cmd, b = msgs[i]
    if cmd == 0x17:
        return True
    return any(reach(v, target, depth + 1) for v, _ in ops(cmd, b))


cases = collections.defaultdict(collections.Counter)
extra = collections.defaultdict(collections.Counter)
for line in open(sys.argv[2]):
    o = json.loads(line)
    if o.get('poke') != 'msg_eval':
        continue
    w, i, r = o['a'], o['b'], o['c']
    cmd, b = msgs[i]
    if cmd == 0x17:
        cases[cmd][f'random pick: result - index = {r - i}'] += 1
        continue
    if cmd == 0x10:
        extra[cmd][f'kind {b[5]}'] += 1
    if cmd == 0x23:
        extra[cmd][f'kind {b[5]}'] += 1
    hits = [name for v, name in ops(cmd, b) if v == r]
    if hits:
        cases[cmd][' / '.join(sorted(set(hits))) + f' (window {w})'] += 1
        continue
    via = sorted({name for v, name in ops(cmd, b) if reach(v, r)})
    cases[cmd][('chained via ' + ' / '.join(via) if via else 'result not an operand') + f' (window {w})'] += 1
for cmd in sorted(cases):
    print(f'command 0x{cmd:02X}: ' + json.dumps(dict(cases[cmd])) + (f'  operand kinds {dict(extra[cmd])}' if extra[cmd] else ''))
