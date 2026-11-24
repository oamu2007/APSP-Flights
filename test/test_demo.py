"""Integracion con OpenFlights real y comprobacion independiente de rutas."""
import csv
import heapq
import json
import math
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import demo


def dijkstra(adjacency, source):
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
    return distances


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

    def test_expanded_selection_and_directed_routes(self):
        dataset = ROOT / "dataset"
        historical = [node["code"] for node in self.backend.graph["nodes"]]
        ids, seen = {}, set()
        with (dataset / "datos_aeropuertos_validacion.dat").open(encoding="utf-8", newline="") as data:
            for row in csv.reader(data):
                try:
                    airport_id, code = int(row[0]), row[4]
                    lat, lon = float(row[6]), float(row[7])
                except (ValueError, IndexError):
                    continue
                if len(code) == 3 and abs(lat) <= 90 and abs(lon) <= 180:
                    if airport_id not in ids and code not in seen:
                        ids[airport_id] = code
                        seen.add(code)
        connections = set()
        with (dataset / "datos_rutas_validacion.dat").open(encoding="utf-8", newline="") as data:
            for row in csv.reader(data):
                try:
                    source, target = int(row[3]), int(row[5])
                    stops = int(row[7])
                except (ValueError, IndexError):
                    continue
                if len(row) >= 9 and stops == 0 and source != target and source in ids and target in ids:
                    connections.add((source, target))
        degrees = Counter(airport for edge in connections for airport in edge)
        prefix = set(historical)
        ranked = sorted((airport for airport in degrees if ids[airport] not in prefix),
                        key=lambda airport: (-degrees[airport], airport))
        expected = historical + [ids[airport] for airport in ranked[:350]]

        expanded = demo.Backend(preset="expanded")
        try:
            self.assertEqual([node["code"] for node in expanded.graph["nodes"]], expected)
            self.assertEqual(len(expanded.codes), 500)
            actual_edges = {(source, target) for source, target, _ in expanded.graph["edges"]}
            self.assertEqual(len(actual_edges), len(expanded.graph["edges"]))
            self.assertEqual(actual_edges, {(ids[a], ids[b]) for a, b in connections
                                            if ids[a] in expanded.codes and ids[b] in expanded.codes})
            adjacency = {code: [] for code in expected}
            for source, target, km in expanded.graph["edges"]:
                adjacency[source].append((target, km))
            for source, target in (("MEX", expected[-1]), (expected[-1], "MEX"),
                                   (expected[150], "NRT"), ("NRT", expected[150])):
                result = expanded.query(source, target)
                with self.subTest(source=source, target=target):
                    self.assertAlmostEqual(result["distance_km"], dijkstra(adjacency, source)[target], delta=1e-6)
                    self.assertEqual((result["path"][0], result["path"][-1]), (source, target))
                    self.assertTrue(all(hop in actual_edges for hop in zip(result["path"], result["path"][1:])))
        finally:
            expanded.close()

    def test_expanded_prefix_and_limit(self):
        expanded = demo.Backend(151, "expanded")
        try:
            self.assertEqual(expanded.graph["nodes"][:150], self.backend.graph["nodes"])
            self.assertEqual(len(expanded.codes), 151)
        finally:
            expanded.close()
        with self.assertRaisesRegex(RuntimeError, "supports 1 to 500"):
            demo.Backend(501, "expanded")

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
        adjacency = {code: [] for code in nodes}
        weights = {}
        for a, b, km in graph["edges"]:
            adjacency[a].append((b, km))
            weights[a, b] = km
            lat1, lat2 = map(math.radians, (nodes[a]["lat"], nodes[b]["lat"]))
            dlon = math.radians(nodes[b]["lon"]-nodes[a]["lon"])
            h = math.sin((lat2-lat1)/2)**2 + math.cos(lat1)*math.cos(lat2)*math.sin(dlon/2)**2
            expected = 2*6371*math.asin(math.sqrt(max(0, min(1, h))))
            self.assertAlmostEqual(km, expected, places=5)

        # The C++ JSON protocol rounds weights and distances to 12 significant digits.
        tolerance_km = 1e-6
        for source in nodes:
            expected_distances = dijkstra(adjacency, source)
            for target, expected in expected_distances.items():
                with self.subTest(source=source, target=target):
                    result = self.backend.query(source, target)
                    distance, path = result["distance_km"], result["path"]
                    if math.isinf(expected):
                        self.assertIsNone(distance)
                        self.assertEqual(path, [])
                        continue
                    self.assertIsNotNone(distance)
                    self.assertTrue(math.isfinite(distance) and distance >= 0)
                    self.assertAlmostEqual(distance, expected, delta=tolerance_km)
                    self.assertTrue(path)
                    self.assertEqual((path[0], path[-1]), (source, target))
                    if source == target:
                        self.assertEqual((distance, path), (0, [source]))
                    hops = list(zip(path, path[1:]))
                    for hop in hops:
                        self.assertIn(hop, weights)
                    self.assertAlmostEqual(
                        math.fsum(weights[hop] for hop in hops), distance, delta=tolerance_km
                    )

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
