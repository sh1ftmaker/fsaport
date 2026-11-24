#!/usr/bin/env python3
"""Serve build/web with HTTP Range support (the page reads the disc in blocks).

    python3 tools/serve.py [--port 8790] [--dir build/web] [--iso path/to/your.iso]

With --logs DIR the page's log is collected: it POSTs its lines to /log and
each page load lands in DIR/<time>-<id>.log (DIR/latest.log points at the
newest). Only for this machine's own test server.

With --iso the image is also served at /local.iso, for testing with
http://127.0.0.1:8790/?iso=local.iso. Local testing only: never expose a disc.

With --record-manifest a page opened with ?discrecord=1 POSTs the disc blocks
the game read to /disc-manifest, merged into disc-manifest.json in --dir; pages
then fetch those blocks up front and read the rest on demand. The manifest is
block numbers only, no disc data."""
import argparse, datetime, http.server, json, os, re, threading

class Handler(http.server.SimpleHTTPRequestHandler):
    # Keep-alive, so the page's many range reads reuse their connections.
    protocol_version = 'HTTP/1.1'
    iso = None
    def translate_path(self, path):
        if self.iso and path.split('?')[0] == '/local.iso':
            return self.iso
        return super().translate_path(path)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('Accept-Ranges', 'bytes')
        super().end_headers()
    def guess_type(self, path):
        return 'application/wasm' if str(path).endswith('.wasm') else super().guess_type(path)
    def do_GET(self):
        rng = self.headers.get('Range')
        path = self.translate_path(self.path)
        m = re.match(r'bytes=(\d+)-(\d*)', rng or '')
        if not m or not os.path.isfile(path):
            return super().do_GET()
        size = os.path.getsize(path)
        start = int(m.group(1)); end = min(int(m.group(2) or size - 1), size - 1)
        if start > end:
            self.send_response(416)
            self.send_header('Content-Range', f'bytes */{size}')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        self.send_response(206)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length', str(end - start + 1))
        self.end_headers()
        with open(path, 'rb') as f:
            self.wfile.flush()
            self.connection.sendfile(f, start, end - start + 1)
    def copyfile(self, source, outputfile):
        # sendfile: the whole image goes out several times faster than
        # through copyfileobj's small buffer.
        outputfile.flush()
        self.connection.sendfile(source)
    def log_message(self, *a):
        pass

    logs = None
    lock = threading.Lock()
    files = {}
    record_manifest = False
    def post_manifest(self):
        n = int(self.headers.get('Content-Length') or 0)
        if n <= 0 or n > 1 << 20:
            self.send_error(413)
            return
        try:
            got = json.loads(self.rfile.read(n))
            size, gid = int(got['size']), str(got['id'])[:16]
            block = int(got['block'])
            blocks = {int(b) for b in got['blocks']}
        except (ValueError, KeyError, TypeError):
            self.send_error(400)
            return
        with self.lock:
            old = None
            try:
                with open('disc-manifest.json') as f:
                    old = json.load(f)
            except (OSError, ValueError):
                pass
            if old and old.get('size') == size and old.get('id') == gid and old.get('block') == block:
                blocks |= set(old.get('blocks', []))
            out = {'id': gid, 'size': size, 'block': block, 'blocks': sorted(blocks)}
            with open('disc-manifest.json.tmp', 'w') as f:
                json.dump(out, f)
            os.replace('disc-manifest.json.tmp', 'disc-manifest.json')
        self.send_response(204)
        self.end_headers()

    def do_POST(self):
        if self.path.split('?')[0] == '/disc-manifest' and self.record_manifest:
            self.post_manifest()
            return
        if self.path.split('?')[0] != '/log' or not self.logs:
            self.send_error(404)
            return
        n = int(self.headers.get('Content-Length') or 0)
        if n <= 0 or n > 1 << 20:
            self.send_error(413)
            return
        body = self.rfile.read(n).decode('utf-8', 'replace')
        sid, _, text = body.partition('\n')
        sid = re.sub(r'[^A-Za-z0-9-]', '', sid)[:32] or 'anon'
        with self.lock:
            name = self.files.get(sid)
            if name is None:
                stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
                name = self.files[sid] = os.path.join(self.logs, f'{stamp}-{sid}.log')
                latest = os.path.join(self.logs, 'latest.log')
                try:
                    os.remove(latest)
                except FileNotFoundError:
                    pass
                os.symlink(os.path.basename(name), latest)
            with open(name, 'a') as f:
                f.write(text if text.endswith('\n') else text + '\n')
        self.send_response(204)
        self.end_headers()

ap = argparse.ArgumentParser()
ap.add_argument('--port', type=int, default=8790)
ap.add_argument('--dir', default='build/web')
ap.add_argument('--iso')
ap.add_argument('--logs')
ap.add_argument('--record-manifest', action='store_true')
a = ap.parse_args()
Handler.record_manifest = a.record_manifest
Handler.iso = os.path.abspath(a.iso) if a.iso else None
if a.logs:
    Handler.logs = os.path.abspath(a.logs)
    os.makedirs(Handler.logs, exist_ok=True)
os.chdir(a.dir)
http.server.ThreadingHTTPServer(('127.0.0.1', a.port), Handler).serve_forever()
