#!/usr/bin/env python3
"""Writes the fntrace batch 3 harness scripts (docs/HARNESS.md "Diagnostic pokes",
logs/fntrace-answers/batch3/README.md) next to this file:

  python3 tools/harness_scripts/batch3/gen.py

Each script starts the stage through ?stage=S-L (the page URL) and uses the
diagnostics-only `set` keys, so it runs in an FSA_DIAG build (build/diag) only.
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
ALL = [0, 1, 2, 3]


def tap(button, pads=None, gap=20):
    s = [{"hold": [button], "reads": 4}, {"wait_reads": gap}]
    if pads:
        s[0]["pads"] = pads
    return s


def stick(x, y, reads, pads=None, buttons=None):
    s = {"stick": [x, y], "reads": reads}
    if pads:
        s["pads"] = pads
    if buttons:
        s["buttons"] = buttons
    return [s]


def dialogue(n=24, pads=None):
    """The stage-1-0 fairy dialogue: A every 40 reads."""
    out = []
    for _ in range(n):
        out += tap("A", pads, 36)
    return out


def walk(pads=None, rounds=2):
    """Walk the four directions and diagonals with B swings."""
    out = []
    for _ in range(rounds):
        out += stick(127, 0, 90, pads) + tap("B", pads)
        out += stick(0, -127, 60, pads) + tap("B", pads)
        out += stick(-127, 0, 120, pads) + tap("B", pads)
        out += stick(0, 127, 60, pads) + tap("B", pads)
        out += stick(90, 90, 60, pads) + tap("B", pads)
        out += stick(-90, -90, 60, pads) + tap("B", pads)
    return out


def start(extra=None):
    s = [{"comment": "diagnostic pokes before the stage builds", "set": dict(extra or {})}] if extra else []
    return s + [{"wait": {"mode": 7}, "timeout": 6000}, {"wait_reads": 300}]


def write(name, steps, comment):
    doc = {"version": 1, "comment": comment, "steps": steps + [{"end": True}]}
    with open(os.path.join(HERE, name + ".json"), "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")


# 1. Hit flash and actor matrix in stage 1-0: every Octorok / SandCrab is hit
#    (takeHit_801F37AC, attack 0: the flash without a kill), with hit types 0 and 5
#    and from below (knock-back up: fn_801F172C's top edge); then shaken
#    (fn_801F3780(300): mSpriteParams.mMtx = a translation) and killed by player 0
#    (fn_801A3C50: the death shrink, mMtx = a scale); one is moved to layer 8.
hits = []
for rnd in range(6):
    for tgt in ("OKTA", "RCK2"):
        hits.append({"set": {"target": tgt, "hit_type": 5 if rnd % 2 else 0, "hit_dy": 16 if rnd < 4 else -16,
                             "hit_strength": 1 + rnd % 2, "hit": 0}})
    hits += [{"wait_reads": 50}]
write("hitflash_1_0", start({"force": 1}) + dialogue() + walk(rounds=1) + hits + [
    {"set": {"target": "OKTA", "shake": 300}}, {"set": {"target": "RCK2", "shake": 300}}, {"wait_reads": 200},
    {"set": {"target": "OKTA", "layer": 8}}, {"wait_reads": 60}, {"set": {"target": "OKTA", "layer": 0}},
    {"set": {"target": "OKTA", "kill": 0}}, {"wait_reads": 200},
] + walk(rounds=2), "batch 3: hit flash, actor matrix, fn_801F172C flag (force 1), stage 1-0")

# 2. fn_801F172C with flag forced on everywhere, a long walk with hits (stage 1-0).
write("force172c_1_0", start({"force": 1}) + dialogue() + walk(rounds=4) + hits + walk(rounds=4),
      "batch 3: fn_801F172C corner block (force bit 0: sweepMapBox passes flag = true), stage 1-0")

# 3. LeafEffectNPC::update: player 0 held in the fn_8023A51C state (field_0xb9c) at
#    the start (next to the leaves at x 48..336, y 384..464), then walking past them.
write("leaves_1_0", start() + dialogue() + [{"set": {"player_b9c": 1}}, {"wait_reads": 30},
      {"set": {"player_b9c": 0}}, {"wait_reads": 60}] + stick(127, 0, 30) + [{"set": {"player_b9c": 1}}]
      + stick(127, 0, 240) + stick(0, -127, 60) + stick(-127, 0, 240) + [{"set": {"player_b9c": 0}}] + walk(rounds=1),
      "batch 3: LeafEffectNPC spawnLeaves (player 0 field_0xb9c held), stage 1-0")

# 4. Four players (no GBA: kind 2 each), all four pads driven alike, stage 1-0.
write("players4_1_0", start({"players_on_stage": 15, "pads": 15}) + dialogue(pads=ALL) + walk(ALL, rounds=3),
      "batch 3: four players with controllers (players_on_stage 15, pads 15), stage 1-0")

# 5. Four players, players 1-3 on faked linked GBAs (kind 1), stage 1-0.
write("gba4_1_0", start({"players_on_stage": 15, "kinds_on_stage": 0x56, "pads": 1, "gba_link": 14})
      + dialogue() + walk(rounds=3),
      "batch 3: player 0 on a controller, players 1-3 on faked linked GBAs (gba_link 14), stage 1-0")

# 1b. Stage 1-2 starts with actors on layers 8, 9, 12, 13 (NIWA, TINB, PONT, RUPY, WARP):
#     drawSprite_801F06F0's layer-8 branch without any poke.
write("layer8_1_2", start() + walk(rounds=2), "batch 3: drawSprite layer 8 (no poke), stage 1-2")

# 6. The loaded BMG's conditional messages: list them (msg_scan).
write("msgscan", start() + [{"set": {"msg_scan": 1}}, {"wait_reads": 60}],
      "batch 3: list the messages that start with a conditional command (msg_scan)")


# 7. With a msg_scan run's harness_msg.jsonl: fn_80383EB0(window, index) for every
#    conditional message whose operands cannot lead to a 0x2A command (fn_80383E2C
#    returns 0x2A messages but fn_80383EB0's switch has no case for it, in the
#    original's jump table too: the loop would never end), for windows 0-3.
def msgeval(scan, name, windows=(0, 1, 2, 3), players=15):
    msgs = {}
    for line in open(scan):
        o = json.loads(line)
        msgs[o["index"]] = (o["command"], bytes.fromhex(o["bytes"]))

    def targets(b):
        return {int.from_bytes(b[k:k + 4], "big") for k in range(5, len(b) - 3)}

    bad = {i for i, (c, _) in msgs.items() if c == 0x2A}
    changed = True
    while changed:
        changed = False
        for i, (c, b) in msgs.items():
            if i not in bad and (c == 0x17 or targets(b) & bad):
                if c == 0x17 or targets(b) & bad:
                    if c != 0x17 or any(j in bad for j in range(i + 1, i + 1 + b[5])):
                        bad.add(i)
                        changed = True
    steps = []
    for w in windows:
        steps.append({"set": {"msg_window": w}})
        steps += [{"set": {"msg_eval": i}} for i in sorted(msgs) if i not in bad]
    write(name, start({"players_on_stage": players, "pads": 15} if players != 1 else None) + steps + [{"wait_reads": 60}],
          f"batch 3: fn_80383EB0 for the conditional messages of {os.path.basename(scan)} (skipped: {sorted(bad)})")
    return sorted(bad)


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 2:  # gen.py SCAN.jsonl NAME [PLAYERS_MASK]
        print("skipped:", msgeval(sys.argv[1], sys.argv[2], players=int(sys.argv[3]) if len(sys.argv) > 3 else 1))
