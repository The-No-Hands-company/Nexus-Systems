import json
import os
import sys
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse

PORT = 9900
DATA_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "../ecosystem-visualizer/ecosystem-data.json"))
LAYOUT_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "layout.json"))

class VisionBoardHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Allow CORS
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200, "OK")
        self.end_headers()

    def do_GET(self):
        parsed_path = urlparse(self.path)
        if parsed_path.path == "/api/data":
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            
            # Read ecosystem-data.json
            eco_data = {}
            if os.path.exists(DATA_PATH):
                try:
                    with open(DATA_PATH, "r", encoding="utf-8") as f:
                        eco_data = json.load(f)
                except Exception as e:
                    print(f"Error reading ecosystem data: {e}")
            
            # Read layout.json
            layout = {
                "nodePositions": {},
                "customConnections": [],
                "hiddenConnections": [],
                "commentBoxes": [],
                "rerouteNodes": []
            }
            if os.path.exists(LAYOUT_PATH):
                try:
                    with open(LAYOUT_PATH, "r", encoding="utf-8") as f:
                        layout = json.load(f)
                except Exception as e:
                    print(f"Error reading layout data: {e}")
            
            response = {
                "ecosystemData": eco_data,
                "layout": layout
            }
            self.wfile.write(json.dumps(response, indent=2).encode("utf-8"))
        else:
            super().do_GET()

    def do_POST(self):
        parsed_path = urlparse(self.path)
        if parsed_path.path == "/api/layout":
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            try:
                layout_data = json.loads(post_data.decode("utf-8"))
                with open(LAYOUT_PATH, "w", encoding="utf-8") as f:
                    json.dump(layout_data, f, indent=2)
                self.send_response(200)
                self.send_header("Content-type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"status": "ok"}).encode("utf-8"))
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"status": "error", "message": str(e)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

def run(server_class=HTTPServer, handler_class=VisionBoardHandler):
    # Ensure layout.json directory exists
    os.makedirs(os.path.dirname(LAYOUT_PATH), exist_ok=True)
    server_address = ('', PORT)
    httpd = server_class(server_address, handler_class)
    print(f"Serving Ecosystem Vision Board at http://localhost:{PORT}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()

if __name__ == "__main__":
    # Change directory to the script's directory to serve files correctly
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    run()
