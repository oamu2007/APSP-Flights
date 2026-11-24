# APSP-Flights evaluation

The default graph has 150 airports and 6,552 directed edges. Floyd–Warshall agreed with independent Dijkstra on all 22,500 ordered pairs, including identity queries. Every distinct pair was reachable. Each single-airport removal preserved reachability among survivors, although removing PEK lengthened 1,345 routes. The largest shortest-path distance was **20,093.22 km**, between BOG and CGK in both directions.

## Data and graph

The evaluation uses the bundled `dataset/datos_aeropuertos_validacion.dat` (7,698 rows), `dataset/datos_rutas_validacion.dat` (67,663 rows), and the fixed 150 IATA codes in `dataset/aeropuertos_demo.txt`, in file order. The repository records an October 20, 2026 download. OpenFlights says its route provider stopped updating the route data in June 2014. File checksums, attribution, and license references accompany the bundled data. [Repository data notes](../dataset/README.md) · [OpenFlights data documentation](https://openflights.org/data.php)

The demo loader keeps airports with usable IATA codes, IDs, and coordinates. It includes zero-stop route rows whose airport IDs are both selected, excludes self-edges, and collapses duplicate directed connections. It assigns edge weights using Haversine great-circle distance in kilometers and a spherical Earth radius of 6,371 km. The selection starts with MEX and NRT and fills the other 148 places by source-network in-degree plus out-degree, breaking ties by OpenFlights airport ID. That rule favors well-connected airports. Requiring both endpoints to be selected also excludes any path that would use an airport outside the list. [Selection method](../README.md#que-150-aeropuertos-estamos-usando)

| Metric | Result |
| --- | ---: |
| Airports | 150 |
| Directed edges | 6,552 |
| Directed density, `6,552 / (150 × 149)` | 29.32% |
| Directed edges with no reverse edge | 30 |
| Sum of in-degrees and sum of out-degrees | 6,552 each |
| Mean in-degree and mean out-degree | 43.68 each |
| Highest in-degree | AMS, 110 |
| Highest out-degree | FRA, 110 |
| Strongly connected components | 1 (150 airports) |
| Reachable / unreachable ordered pairs, excluding self-pairs | 22,350 / 0 |
| OpenFlights country or territory labels | 54 |

The four most represented labels were United States (28), China (14), United Kingdom (10), and Spain (9). Latitudes ranged from 33.95° S (SYD) to 60.32° N (HEL). Longitudes ranged from 123.18° W (YVR) to 151.18° E (SYD). SCCs were computed by grouping mutually reachable vertices in the Floyd–Warshall matrix. [Graph totals](artifacts/graph_summary.csv) · [Airport degrees and coordinates](artifacts/airport_degrees.csv) · [Component sizes](artifacts/components.csv)

## Shortest-path correctness

The Python integration test ran heap-based Dijkstra from each source using the directed edges exported by the demo. It compared reachability and distance with the C++ backend for every destination and verified each reconstructed path's endpoints, directed hops, and sum of weights. Distance checks used an absolute tolerance of `1e-6 km` to account for floating-point arithmetic and JSON serialization at 12 significant digits. All comparisons passed. The 150 diagonal queries returned exact zero and a single airport. The two-airport fixture checks an unreachable MEX → NRT and the reachable reverse direction. The suite also checks asymmetric distances on the default graph.

The C++ tie fixture verifies that a direct edge survives an exactly equal alternative and that the first equal-cost path found in vertex order is retained. The solver replaces a predecessor only on a strict improvement. The constructor initializes two `V × V` matrices, processes `E` edges, and executes `V³` relaxations. Its time is `Θ(V³ + E)` and its auxiliary space is `Θ(V²)`. A distance query is `Θ(1)` and reconstructing a finite route takes time proportional to the number of airports on it. The independent comparison tests the solver on the loaded graph. The same Python test also recomputes the Haversine weights from exported coordinates.

## Single-airport removal

For each airport, the analysis deleted the vertex and its incident edges, kept the relative order of survivors, and reran the same Floyd–Warshall constructor. It compared distinct directed pairs with both endpoints surviving. The loss share divides newly unreachable pairs by pairs that were reachable before deletion. The CSV ranks by loss count descending, then by IATA code ascending. Among pairs that stayed reachable, an increase greater than `1e-6 km` counts as a detour. The detour share divides this count by the number still reachable. Mean and maximum detours summarize the increased pairs only. Shares with a zero denominator and detour magnitudes with no increases are blank.

All 150 removals tied at **0 lost out of 22,052** previously reachable surviving pairs. There were measurable detours in 137 removal scenarios. PEK produced the largest detour count, with **1,345 affected pairs (6.10%)** and a mean increase of 390.25 km among those pairs. GRU produced the largest single increase, **5,052.53 km**, while affecting 14 pairs. The focused directed fixture verifies both reachability loss and distance increases, and a sparse fixture exercises an empty comparison set. [All 150 removal results](artifacts/airport_removal.csv)

## Longest shortest paths

These are the ten largest finite shortest-path distances between distinct airports. The ranking uses descending full-precision distance and breaks exact ties by ascending source and destination IATA codes. Each distance shown is the sum along the reconstructed route. The first row is the maximum over reachable directed pairs.

| Rank | Airport sequence | Distance (km) | Hops |
| ---: | --- | ---: | ---: |
| 1 | BOG → IAH → NRT → CGK | 20,093.22 | 3 |
| 2 | CGK → NRT → IAH → BOG | 20,093.22 | 3 |
| 3 | BOG → FRA → SIN | 19,366.36 | 2 |
| 4 | SIN → FRA → BOG | 19,366.36 | 2 |
| 5 | CGK → NRT → IAH → PTY | 19,364.73 | 3 |
| 6 | PTY → IAH → NRT → CGK | 19,364.73 | 3 |
| 7 | SYD → AUH → CMN → LPA → TFS | 19,190.67 | 4 |
| 8 | TFS → LPA → CMN → AUH → SYD | 19,190.67 | 4 |
| 9 | BOG → FRA → KUL | 19,084.62 | 2 |
| 10 | KUL → FRA → BOG | 19,084.62 | 2 |

The longest direct edge was **SYD → DFW, 13,808.18 km**, ranked using the same endpoint tie rule. [Ranked paths](artifacts/longest_shortest_paths.csv) · [Longest direct edge](artifacts/longest_direct_edge.csv)

## Runtime experiment

The benchmark used the historical 150-node graph and one deterministic synthetic graph per size. Each synthetic graph samples `round(0.29315436241610737 × V(V−1))` distinct ordered non-self edges uniformly without replacement. It traverses ordered pairs in vertex order and samples using MT19937 seeded with `20261124 + V`, with rejection sampling for bounded draws. Each selected edge receives a uniform integer weight from 1 to 10,000 synthetic km. Historical density is matched up to integer rounding. Synthetic weights and topology have no geographic constraints.

The benchmark ran sequentially on November 24, 2026, after the parallel evaluation jobs finished. The host was an Intel Core i7-1255U with 12 logical processors visible to the process, running Windows x64 build 26300.10075. The C++17 build used GCC 15.2.0 (MinGW-w64/UCRT), CMake 4.3.1, and Ninja 1.13.2 with `Release` flags `-O3 -DNDEBUG`. Each graph had two untimed warmups and nine measured trials. A monotonic `std::chrono::steady_clock` timed only the solver constructor, including distance and predecessor matrix allocation and initialization, direct-edge insertion, and relaxation. Graph preparation, result checksumming, solver destruction, file output, and plotting were outside the timed interval.

| Graph | Nodes | Edges | Median (ms) | Min–max (ms) | Sample SD (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Synthetic | 75 | 1,627 | 0.461 | 0.428–0.518 | 0.035 |
| Historical | 150 | 6,552 | 2.192 | 2.105–2.525 | 0.138 |
| Synthetic | 150 | 6,552 | 3.279 | 3.024–3.338 | 0.138 |
| Synthetic | 250 | 18,249 | 14.080 | 12.679–16.169 | 1.006 |
| Synthetic | 400 | 46,787 | 47.248 | 46.234–50.497 | 1.260 |
| Synthetic | 550 | 88,518 | 115.137 | 113.738–122.362 | 3.315 |
| Synthetic | 750 | 164,679 | 275.461 | 268.372–418.651 | 47.998 |

![Log–log runtime plot with trials, medians, min–max ranges, and a cubic reference](artifacts/runtime.svg)

The cubic reference is `3.2792 ms × (V / 150)³`, anchored at the synthetic 150-node median. The measured synthetic median grew about 598-fold from 75 to 750 nodes across a tenfold increase in graph size. The reference grows 1,000-fold over the same interval. The solver performs `V³` relaxations, while elapsed time also reflects the graph instance, memory and processor behavior, and system load. The two 150-node instances had the same edge count and different medians. The nine 750-node trials had the widest spread. These timings describe one host, build, and graph instance per size. [Trials](artifacts/benchmark_trials.csv) · [Summaries with means and sample standard deviations](artifacts/benchmark_summary.csv)

## Reproduction

Run from the repository root with a C++17 compiler, CMake 3.20+, Ninja, and Python 3.10+. The evaluation and plot need no additional Python packages. The commands use PowerShell with MinGW. On Linux use `./build/bin/apsp_analysis`, `./build/bin/apsp_benchmark`, and `python3`.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
py -3 -m unittest discover -s test -p 'test_*.py' -v
py -3 scripts/demo.py --headless --from MEX --to NRT
.\build\bin\apsp_analysis.exe build/evaluation-run
.\build\bin\apsp_benchmark.exe build/evaluation-run
py -3 evaluation/plot_runtime.py build/evaluation-run
```

CTest passed both targets and Python passed all ten tests on Windows. The headless smoke test returned MEX → LAX → NRT at 11,253.82 km through both Python and the Windows launcher. The optional `py -3 scripts/demo.py --gui-smoke` check also passed with Tkinter. On this host, the six graph CSVs regenerated byte-for-byte from the checked-in inputs. Passing the published timing CSVs to `plot_runtime.py` reproduced the SVG byte-for-byte. A fresh benchmark samples new wall-clock timings. Last-digit differences in geographic distances can arise across math libraries, and the order of nearly equal distances can follow those differences.

The routes describe a historical, hub-heavy induced graph. Schedules, service frequency, capacity, demand, actual flight tracks, and commercial feasibility are outside the model. The removal experiment covers one airport at a time within that graph. Current service resilience and results for other airport selections remain outside this experiment.
