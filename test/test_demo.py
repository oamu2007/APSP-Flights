"""Integracion con OpenFlights real y comprobacion independiente de rutas."""
import heapq
import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import demo


def dijkstra(graph, source, target):
    adjacency = {node["code"]: [] for node in graph["nodes"]}
    for a, b, km in graph["edges"]:
        adjacency[a].append((b, km))
    distances = {node: math.inf for node in adjacency}
    distances[source] = 0
    pending = [(0, source)]
    while pending:
        cost, node = heapq.heappop(pending)
        if cost > distances[node]:
            continue
        for neighbor, km in adjacency[node]:
            if cost + km < distances[neighbor]:
                distances[neighbor] = cost + km
                heapq.heappush(pending, (cost + km, neighbor))
    return distances[target]


class DemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.backend = demo.Backend()

    @classmethod
    def tearDownClass(cls):
        cls.backend.close()

    def test_known_routes(self):
        cases = [("MEX", "NRT", ["MEX","LAX","NRT"], 11253.82),
                 ("NRT", "MEX", ["NRT","MEX"], 11253.73),
                 ("TFS", "HER", ["TFS","LPA","CMN","TUN","MLA","ATH","HER"], 4318.10)]
        for source, target, path, km in cases:
            with self.subTest(source=source, target=target):
                result = self.backend.query(source, target)
                self.assertEqual(result["path"], path)
                self.assertAlmostEqual(result["distance_km"], km, places=2)

    def test_fixed_airports_and_reused_process(self):
        pid = self.backend.process.pid
        first = self.backend.query("MEX", "NRT")
        second = self.backend.query("TFS", "HER")
        self.assertEqual(first["nodes"], second["nodes"])
        self.assertEqual(self.backend.process.pid, pid)
        self.assertEqual(len(first["nodes"]), 150)
        selection = (ROOT/"dataset/aeropuertos_demo.txt").read_text(encoding="utf-8")
        codes = [line.split("#")[0].strip() for line in selection.splitlines()]
        self.assertEqual([node["code"] for node in first["nodes"]], [code for code in codes if code])

    def test_identity(self):
        result = self.backend.query("MEX", "MEX")
        self.assertEqual(result["path"], ["MEX"])
        self.assertEqual(result["distance_km"], 0)

    def test_unknown_airport_does_not_change_graph(self):
        for code in ("ZZZZ", "GKA"):
            with self.subTest(code=code), self.assertRaises(ValueError):
                self.backend.query("MEX", code)
        self.assertEqual(len(self.backend.query("MEX","NRT")["nodes"]),150)

    def test_two_airports(self):
        backend = demo.Backend(2)
        try:
            result = backend.query("MEX", "NRT")
            self.assertEqual([node["code"] for node in result["nodes"]], ["MEX","NRT"])
            self.assertEqual(result["path"], [])
            self.assertIsNone(result["distance_km"])
            self.assertEqual(backend.query("NRT","MEX")["path"], ["NRT","MEX"])
        finally:
            backend.close()

    def test_invalid_limits(self):
        for limit in (0, 151, 1001):
            with self.subTest(limit=limit), self.assertRaises(RuntimeError):
                demo.Backend(limit)

    def test_weights_and_independent_dijkstra(self):
        graph = self.backend.graph
        nodes = {node["code"]: node for node in graph["nodes"]}
        weights = {(a,b):km for a,b,km in graph["edges"]}
        for a, b, km in graph["edges"]:
            lat1, lat2 = map(math.radians, (nodes[a]["lat"], nodes[b]["lat"]))
            dlon = math.radians(nodes[b]["lon"]-nodes[a]["lon"])
            h = math.sin((lat2-lat1)/2)**2 + math.cos(lat1)*math.cos(lat2)*math.sin(dlon/2)**2
            expected = 2*6371*math.asin(math.sqrt(max(0, min(1, h))))
            self.assertAlmostEqual(km, expected, places=5)
        for source, target in (("MEX","NRT"), ("NRT","MEX"), ("MEX","LHR"), ("TFS","HER"), ("MEX","MEX")):
            with self.subTest(source=source, target=target):
                result = self.backend.query(source, target)
                total = sum(weights[a,b] for a,b in zip(result["path"], result["path"][1:]))
                self.assertAlmostEqual(total, result["distance_km"], places=5)
                self.assertAlmostEqual(dijkstra(graph, source, target), result["distance_km"], places=5)

    def test_headless_from_another_directory(self):
        with tempfile.TemporaryDirectory(dir=ROOT/"build") as folder:
            output = Path(folder)/"result.json"
            run = subprocess.run([sys.executable, str(ROOT/"scripts/demo.py"), "--headless", "--output", str(output)],
                                 cwd=folder, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["path"], ["MEX","LAX","NRT"])

    def test_cpp_single_query(self):
        run = subprocess.run([str(demo.binary_path())], cwd=ROOT, capture_output=True, text=True, timeout=20)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(json.loads(run.stdout)["path"], ["MEX","LAX","NRT"])

    def test_selection_file_errors(self):
        with tempfile.TemporaryDirectory(dir=ROOT/"build") as folder:
            selection = Path(folder)/"selection.txt"
            cases = [("MEX\nMEX\n", "duplicado"), ("MEX\nZZZ\n", "no encontrado"),
                     ("MEX\n", "menos aeropuertos")]
            for content, message in cases:
                with self.subTest(content=content):
                    selection.write_text(content, encoding="utf-8")
                    run = subprocess.run([str(demo.binary_path()), "--selection", str(selection), "--limit", "2"],
                                         cwd=ROOT, capture_output=True, text=True, timeout=20)
                    self.assertNotEqual(run.returncode, 0)
                    self.assertIn(message, run.stderr)


if __name__ == "__main__":
    unittest.main()
