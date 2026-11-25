#!/usr/bin/env python3
"""Turn the page's GBA screen shots into files (deluxe; docs/MULTIPLAYER.md, "GBA screen").

The page logs one line per shot, `GBASHOT <name> <port> <json>`, from
`fsaGba.logShot(name, port)` (platform/web/fsa_gba.js): the native and the
GameCube-drawn screen of the same frame and a side-by-side picture with their
difference. Given web_run.mjs's console.log this writes, per shot,

    OUT/<name>-p<port>.png          native | GameCube | difference (scale 2)
    OUT/<name>-p<port>-native.png   the native screen (240 x 160)
    OUT/<name>-p<port>-gc.png       the GameCube-drawn screen, when there is one

and prints the differing-pixel counts.

    python3 tools/gba_shots.py build/web-run/console.log docs/multiplayer [--only NAME,...]
"""
import argparse
import base64
import json
import os


def save(url, path):
    if not url:
        return False
    head, data = url.split(',', 1)
    with open(path, 'wb') as f:
        f.write(base64.b64decode(data))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('out')
    ap.add_argument('--only', default='')
    ap.add_argument('--no-singles', action='store_true', help='only the side-by-side picture')
    a = ap.parse_args()
    only = set(x for x in a.only.split(',') if x)
    os.makedirs(a.out, exist_ok=True)
    for line in open(a.log, errors='replace'):
        i = line.find('GBASHOT ')
        if i < 0:
            continue
        _, name, port, js = line[i:].rstrip('\n').split(' ', 3)
        if only and name not in only:
            continue
        r = json.loads(js)
        base = os.path.join(a.out, f'{name}-p{port}')
        save(r['png'], base + '.png')
        if not a.no_singles:
            save(r['native'], base + '-native.png')
            save(r['gc'], base + '-gc.png')
        d = r['diff']
        what = 'no GameCube view' if d < 0 else f"{d} of {r['total']} pixels differ ({100.0 * d / r['total']:.2f} %)"
        print(f"{name} port {port} read {r['reads']}: {what} -> {base}.png")


if __name__ == '__main__':
    main()
