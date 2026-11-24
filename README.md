# APSP-Flights

APSP-Flights finds the shortest routes by total distance through a directed graph of historical OpenFlights connections. The C++17 library loads the graph, assigns Haversine weights in kilometers, and computes all-pairs distances and predecessors with Floyd–Warshall. Python provides the Tkinter window and terminal interface.

## Run the application

On Windows, run the launcher from the checkout or double-click it. It builds the C++ backend with CMake and opens the window. The data is bundled, and the Python interface uses the standard library.

```powershell
.\apsp-flights.cmd
.\apsp-flights.cmd --headless --from TFS --to HER
.\apsp-flights.cmd --preset expanded --list
```

In the window, select the historical or expanded graph, enter IATA codes, and choose **Find route**. The **Airports** tab filters by code, city, country, or name. The route map uses gray dots for active airports and green arrows for the selected path. It plots longitude and latitude on a schematic and splits connections that cross the 180th meridian.

<a name="que-150-aeropuertos-estamos-usando"></a>

## Airport selections

The default graph uses the 150 codes in `dataset/aeropuertos_demo.txt` in file order. The original selection starts with MEX and NRT, then ranks 148 airports by the sum of distinct directed in- and out-connections, with ties broken by OpenFlights airport ID. Its first ten entries are MEX, NRT, FRA, CDG, AMS, ISL, ATL, ORD, PEK, and MUC. The fixed study uses these inputs and their 6,552 directed edges. [Evaluation report](evaluation/README.md)

Choose **Expanded (500)** or `--preset expanded` to keep the fixed prefix and add airports ranked by degree in the full usable OpenFlights network. This selection contains 20,001 directed connections. Both graphs are induced subgraphs. The loader includes a connection only when both endpoints belong to the selection. [Selection policy and measured startup cost](dataset/README.md#expanded-airport-selection)

`--limit N` takes the first N airports of the chosen selection. Use `--limit 2` with the historical preset to load MEX and NRT. MEX → NRT is unreachable in that graph, and the reverse connection is available. The expanded preset supports up to 500 airports. C++ also accepts an alternative `--selection` file containing one unique, usable IATA code per line. An explicit historical limit allows up to 1,000 codes from a custom list.

## Airport removal scenarios

Enter an airport's IATA code and choose **Remove airport**. The application removes the airport and its incident directed edges, then recalculates routes on the surviving graph in the background. The route selectors, airport table, and map update with the scenario. An attempted query with a removed endpoint shows a message, and mutually unreachable surviving airports can still be queried in either direction. Choosing a different airport replaces the current removal using the original selection. **Restore baseline** returns the original airports and routes. Switching presets starts from the new preset's baseline.

The comparison uses ordered pairs of distinct surviving airports. The loss share divides newly unreachable pairs by previously reachable surviving pairs. An increase above `1e-6 km` counts as a detour. The detour share uses the number of pairs still reachable, and the mean and maximum use only increased distances. Headless JSON includes `removed_airport` and `impact`, with `null` for undefined shares or detour magnitudes. The window describes empty comparisons and unchanged distances in its impact summary.

```powershell
.\apsp-flights.cmd --headless --remove LAX --from MEX --to NRT
.\apsp-flights.cmd --headless --preset expanded --remove LAX --from MEX --to NRT
```

The equivalent direct C++ command is `build\bin\apsp_demo.exe --remove LAX --from MEX --to NRT`. On Linux, use `./build/bin/apsp_demo`. Run C++ from the repository root so it can resolve the default dataset paths. With `--serve`, the backend emits an initial graph as one JSON line and takes IATA pairs, `--remove IATA`, or `--restore` on standard input. Each removal or restore returns the active graph as JSON. Route requests return the path and distance, with an empty path and `null` distance for unreachable pairs. The Python interface writes the graph and latest successful query to `results/ultima_ruta.json` by default. Use `--output` to change the path.

## Example routes on the fixed selection

| Query | Path | Distance |
| --- | --- | ---: |
| MEX → LAX | MEX → LAX | 2,500.00 km |
| MEX → NRT | MEX → LAX → NRT | 11,253.82 km |
| NRT → MEX | NRT → MEX | 11,253.73 km |
| MEX → LHR | MEX → LHR | 8,903.86 km |
| TFS → HER | TFS → LPA → CMN → TUN → MLA → ATH → HER | 4,318.10 km |
| MEX → MEX | MEX | 0 km |

The solver minimizes kilometers inside the loaded graph. The data reflects historical connections, and the model excludes schedules, prices, capacity, and commercial availability. [Data provenance and license](dataset/README.md)

## Build and test

Use CMake 3.20+, a C++17 compiler, and Python 3.10+. Tkinter is needed to open the window.

On Linux, from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s test -p 'test_*.py' -v
python3 scripts/demo.py
```

On Windows with MinGW and Ninja, use a fresh build directory:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
py -3 -m unittest discover -s test -p 'test_*.py' -v
py -3 scripts/demo.py --gui-smoke
```

Use the Python interpreter configured for the application if `py` is unavailable. Visual Studio builds may put the C++ executable in `build/bin/Release`. The original launcher `iniciar_demo.cmd` forwards to `apsp-flights.cmd`. The executable `apsp_demo`, Python entry point `scripts/demo.py`, and selection filename `dataset/aeropuertos_demo.txt` retain their existing names for compatibility.

See the [ICC006 requirements, test commands, and source inventory](docs/COURSE.md) for course verification.
