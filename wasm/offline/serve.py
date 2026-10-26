#!/usr/bin/env python3
"""Serves this folder on http://127.0.0.1:8123/ and opens it in the default browser.

Needs only Python 3 (no packages, no administrator rights). Stop it with Ctrl+C or by closing
the window. Set NO_BROWSER=1 to skip opening the browser.
"""
import functools
import http.server
import os
import socketserver
import sys
import webbrowser

ROOT = os.path.dirname(os.path.abspath(__file__))
# Saves live in the browser's storage for this exact address, so keep the port stable.
PORTS = [8123, 8124, 8125]


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.js': 'text/javascript',
        '.json': 'application/json',
        '.webmanifest': 'application/manifest+json',
        '.png': 'image/png',
    }

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def log_message(self, *args):
        pass


def main():
    handler = functools.partial(Handler, directory=ROOT)
    httpd = None
    for port in PORTS:
        try:
            httpd = socketserver.ThreadingTCPServer(('127.0.0.1', port), handler)
            break
        except OSError:
            continue
    if httpd is None:
        sys.exit('Ports %s are all in use; close the other copy of the game first.' % PORTS)
    url = 'http://127.0.0.1:%d/' % httpd.server_address[1]
    print("Cracker1000's Mod is running at %s  (press Ctrl+C to stop)" % url, flush=True)
    if not os.environ.get('NO_BROWSER'):
        webbrowser.open(url)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
