import http.server
import json
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.request
from functools import partial
from pathlib import Path


class EndToEndSmokeTests(unittest.TestCase):
    def _free_port(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("127.0.0.1", 0))
            return s.getsockname()[1]

    def test_build_and_serve_graph_artifacts(self):
        repo_root = Path(__file__).resolve().parents[1]
        extractor = repo_root / "extract_graph.py"
        index_html = repo_root / "index.html"
        app_js = repo_root / "app.js"

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            (temp_root / "docs.md").write_text(
                "\n".join(
                    [
                        "system: SmokeSystem",
                        "owns: Graph Parsing",
                        "depends_on: Nexus-Core",
                        "exposes: /api/smoke",
                        "integrates_with: Nexus-Cloud",
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            (temp_root / "index.html").write_text(index_html.read_text(encoding="utf-8"), encoding="utf-8")
            (temp_root / "app.js").write_text(app_js.read_text(encoding="utf-8"), encoding="utf-8")

            result = subprocess.run(
                [sys.executable, str(extractor), "--root", str(temp_root), "--out", str(temp_root / "graph.json")],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            graph = json.loads((temp_root / "graph.json").read_text(encoding="utf-8"))
            self.assertEqual(graph["schemaVersion"], "1.0.0")
            self.assertGreater(graph["nodeCount"], 0)

            port = self._free_port()
            handler = partial(http.server.SimpleHTTPRequestHandler, directory=str(temp_root))
            server = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()

            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{port}/index.html", timeout=5) as resp:
                    html = resp.read().decode("utf-8")
                with urllib.request.urlopen(f"http://127.0.0.1:{port}/graph.json", timeout=5) as resp:
                    served_graph = json.loads(resp.read().decode("utf-8"))
                with urllib.request.urlopen(f"http://127.0.0.1:{port}/app.js", timeout=5) as resp:
                    app_js_content = resp.read().decode("utf-8")
            finally:
                server.shutdown()
                thread.join(timeout=2)
                server.server_close()

            self.assertIn("Ecosystem Graph", html)
            self.assertEqual(served_graph["schemaVersion"], "1.0.0")
            self.assertIn("applyFilters", app_js_content)


if __name__ == "__main__":
    unittest.main()
