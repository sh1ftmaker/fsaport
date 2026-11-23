#!/usr/bin/env python3
"""Serve build/web with HTTP Range support (the page reads the disc in blocks).

    python3 tools/serve.py [--port 8790] [--dir build/web] [--iso path/to/your.iso]

With --iso the image is also served at /local.iso, for testing with
http://127.0.0.1:8790/?iso=local.iso. Local testing only: never expose a disc."""
import argparse, http.server, os, re

class Handler(http.server.SimpleHTTPRequestHandler):
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
        self.send_response(206)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length', str(end - start + 1))
        self.end_headers()
        with open(path, 'rb') as f:
            f.seek(start)
            self.wfile.write(f.read(end - start + 1))
    def log_message(self, *a):
        pass

ap = argparse.ArgumentParser()
ap.add_argument('--port', type=int, default=8790)
ap.add_argument('--dir', default='build/web')
ap.add_argument('--iso')
a = ap.parse_args()
Handler.iso = os.path.abspath(a.iso) if a.iso else None
os.chdir(a.dir)
http.server.ThreadingHTTPServer(('127.0.0.1', a.port), Handler).serve_forever()
