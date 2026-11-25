#!/usr/bin/env python3
"""Which paths of ActorBase::drawSprite_801F06F0 the logged calls of a batch-3 run
took, from the `this` snapshot (port offsets: mLayer_0x4 0x4, mHitTimer_0xd8 0xd8,
mHitTimerStart_0xdc 0xdc, mKnockbackKind_0xf8 0xf8, mHitType_0xfc 0xfc,
mSpriteParams.mMtx 0x138) and the arguments (name, params).

  drawsprite_paths.py RUN_DIR/fntrace.jsonl   (or a split <name>.jsonl)
"""
import collections, json, sys


def word(snaps, off):
    for s in snaps:
        b = bytes.fromhex(s['in'])
        if s['off'] <= off and off + 4 <= s['off'] + len(b):
            return int.from_bytes(b[off - s['off']:off - s['off'] + 4], 'little')
    return None


paths = collections.Counter()
flash = collections.Counter()
mtx = collections.Counter()
ex = {}
for line in open(sys.argv[1]):
    r = json.loads(line)
    if r.get('record') == 'agg' or not r.get('fn_sym', '').startswith('drawSprite_801F06F0'):
        continue
    sn = r.get('snap', [])
    layer = word(sn, 0x4)
    ht, hs, kb, hty, am = (word(sn, o) for o in (0xd8, 0xdc, 0xf8, 0xfc, 0x138))
    if layer is None or ht is None:
        continue
    layer = layer - (1 << 32) if layer >> 31 else layer
    ht = ht - (1 << 32) if ht >> 31 else ht
    name = next((p.get('str') for p in r.get('ptrs', []) if p.get('arg') == 1), None)
    params_null = r['args'][3]['v'] == 0
    if layer > 8:
        k = 'layer > 8: early return'
    else:
        k = []
        k.append('shadow' if am == 0 else 'no shadow (actor mMtx set)')
        if ht > 0:
            total = hs
            excess = total - 0x19
            on = ht > excess or total < 0x19
            k.append('hit flash: ' + ('colour added' if on else 'timer past the flash window') +
                     (', knockback 2 (z fade)' if kb == 2 else '') + (', type 5 (r, g)' if hty == 5 else ', type != 5 (r, g, b)') +
                     (', params copied' if not params_null else ', params NULL'))
            if on:
                v = ht * 0xFF // total if total < 0x19 else (ht - excess) * 0xFF // 25
                flash[(ht, total, v, hty)] += 1
        else:
            k.append('no hit: ' + ('params copied' if not params_null else 'params NULL: sp.field_0x30 = actor\'s'))
            if am:
                k.append('actor mMtx: ' + ('concat with params mMtx' if not params_null and r['ptrs'] and
                                          any(p.get('arg') == 3 and p.get('in') and
                                              int.from_bytes(bytes.fromhex(p['in'])[0x18:0x1c], 'little') for p in r['ptrs'])
                                          else 'sp.mMtx = actor mMtx'))
                mtx[r['caller_name']] += 1
        if layer < 8:
            k.append('layer < 8: drawSobSprite_802F86CC')
        elif name and len(name) > 6 and name[6] != 'g':
            k.append('layer 8: drawSobSprite_801EF574')
        else:
            k.append('layer 8, name[6] == g: nothing drawn')
        k = '; '.join(k)
    paths[k] += 1
    ex.setdefault(k, dict(seq=r['seq'], n=r['n'], caller=r['caller_name'], name=name, layer=layer, hit=ht,
                          start=hs, knockback=kb, type=hty, actor_mtx=hex(am or 0)))
print(json.dumps(dict(paths=paths.most_common(), flash_values=[dict(timer=a, start=b, v=c, type=d, n=n)
                      for (a, b, c, d), n in sorted(flash.items())][:40],
                      actor_mtx_callers=mtx, examples=ex), indent=1))
