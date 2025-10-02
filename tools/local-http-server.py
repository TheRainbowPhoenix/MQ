# Based on a gist by pezcode:
#   https://gist.github.com/pezcode/d51926bdbadcbd4f22f5a5d2fb8e0394

from http.server import HTTPServer, SimpleHTTPRequestHandler
import sys
import os

class Handler(SimpleHTTPRequestHandler):
    extensions_map = {
        '': 'application/octet-stream',
        '.css': 'text/css',
        '.html': 'text/html',
        '.jpg': 'image/jpg',
        '.js':  'application/x-javascript',
        '.json': 'application/json',
        '.manifest': 'text/cache-manifest',
        '.png': 'image/png',
        '.wasm':    'application/wasm',
        '.xml': 'application/xml',
    }

    def do_GET(self):
        org = self.translate_path(self.path)

        # If it doesn't exist as a file, add .html
        if not os.path.exists(org):
            self.path = self.path + ".html"

        SimpleHTTPRequestHandler.do_GET(self)

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        SimpleHTTPRequestHandler.end_headers(self)

# ./local-http-server.py <FOLDER> [PORT]
if __name__ == '__main__':
    http_bind = "0.0.0.0"
    http_dir = sys.argv[1]
    http_port = int(sys.argv[2]) if len(sys.argv) > 2 else 8000

    os.chdir(http_dir)

    with HTTPServer((http_bind, http_port), Handler) as httpd:
        print(f"Serving {http_dir} at http://{http_bind}:{http_port}/ ...")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nKeyboard interrupt received, exiting.")
