# ICC006 course verification

This checklist maps the course's implementation, layout and reproducibility requirements on pages 6–8 to the repository.

| Requirement | Repository evidence |
| --- | --- |
| C or C++ library built with CMake | `CMakeLists.txt` defines the C++17 header-only `apsp` library and the executable and CTest targets. |
| Algorithm header under `include/` | `include/floyd_warshall.hpp` contains Floyd–Warshall and route reconstruction. `include/airport_removal.hpp` implements removal and computes its impact for the application and evaluation. |
| Separate static data loader | `include/openflights_loader.hpp` owns parsing, graph construction and weights. Dataset-dependent C++ tests call `OpenFlightsLoader::load`. The Python tests use that loader through the backend and independently parse or solve inputs for cross-checks. |
| Small test dataset | `dataset/datos_aeropuertos_prueba.csv`, `dataset/datos_rutas_prueba.csv` and `dataset/datos_seleccion_prueba.txt` supply a hand-checkable directed fixture. [Fixture and expected results](../dataset/README.md#small-course-fixture) |
| Original-source validation data | `dataset/datos_aeropuertos_validacion.dat` and `dataset/datos_rutas_validacion.dat` contain the bundled historical OpenFlights CSV records. A fixed selection and the expanded selection policy make experiments repeatable. [Provenance, license and checksums](../dataset/README.md) |
| Insertion, query, deletion and intensive tests | `test/prueba_insercion.cpp`, `test/prueba_consulta.cpp`, `test/prueba_borrado.cpp` and `test/prueba_intensiva.cpp` have individual CTest entries. Additional C++ and Python suites check the algorithm, evaluation and application. |
| CMake-generated build directory | The build commands below generate `build/` from source. The compiled application lives under `build/bin/`. |
| Source, headers, tests and data organized by role | `include/` contains reusable code, `src/main.cpp` contains the C++ command line and server, and `test/` contains automated checks and fixture support. `scripts/` carries the optional Python interface. `evaluation/` contains the experiment entry points, report and reference results. |
| Reproducibility on Unix/Linux | Use the clean CMake build and test commands below. The reproduction section of the evaluation report uses the same fixed data and targets. [Fixed 150-airport evaluation](../evaluation/README.md#reproducción) |

The graph is static. The insertion test exercises the production bulk-load path by checking which vertices and directed edges enter the graph and whether a direct edge enters the solver's route matrix. An airport removal constructs a derived graph and solver. The original graph and solver support restoration.

## Run the checks

From a clean Linux checkout with CMake 3.20+, a C++17 compiler, and Python 3.10+:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -V -R '^prueba_intensiva$'
python3 -m unittest discover -s test -p 'test_*.py' -v
python3 scripts/demo.py --headless --from MEX --to NRT
python3 scripts/demo.py --headless --preset expanded --remove LAX --output build/expanded-removal.json
```

For Windows use the [MinGW/Ninja commands and Tkinter smoke check](../README.md#build-and-test). Run a named operation individually with `ctest --test-dir build -R '^prueba_insercion$'`, replacing the name with `prueba_consulta`, `prueba_borrado` or `prueba_intensiva`.

The intensive test loads the expanded 500-airport, 20,001-edge graph from the original-source validation tables. It verifies two directed queries and reports five measured Floyd–Warshall constructor trials after one warmup. Its median, minimum and maximum describe the local run in milliseconds. Loading, route verification and output are outside the timed interval. The fixed 150-airport study and its archived runtime experiment have their own reproduction commands and sample files. [Evaluation reproduction](../evaluation/README.md#reproducción)

## Source-only submission inventory

| Files | Purpose |
| --- | --- |
| `CMakeLists.txt`, `.gitignore`, `README.md`, `docs/COURSE.md` | Build configuration, project instructions and course checklist |
| `include/floyd_warshall.hpp`, `include/openflights_loader.hpp`, `include/airport_removal.hpp` | Reusable C++17 library |
| `src/main.cpp`, `scripts/demo.py`, `apsp-flights.cmd`, `iniciar_demo.cmd` | Command line, Python application and Windows launchers |
| `test/course_fixture.hpp`, `test/prueba_*.cpp`, `test/test_evaluation.cpp`, `test/test_demo.py` | CTest and Python checks |
| `dataset/README.md`, `dataset/aeropuertos_demo.txt`, `dataset/datos_*_prueba.*`, `dataset/datos_*_validacion.dat` | Data documentation, selection and reproducible inputs |
| `evaluation/README.md`, `evaluation/network.hpp`, `evaluation/analyze.cpp`, `evaluation/benchmark.cpp`, `evaluation/plot_runtime.py`, `evaluation/artifacts/*.csv`, `evaluation/artifacts/runtime.svg` | Published findings, reference outputs and their source |

To package the checked-out revision, inspect its tracked inventory and export it from Git. The command uses the revision's line endings for data checksums and leaves local guidance, executables, Python caches, and generated build and result directories out.

```sh
git ls-tree -r --name-only HEAD
git -c core.autocrlf=false archive --format=zip --prefix=APSP-Flights/ --output=../APSP-Flights-source.zip HEAD
```

The course also calls for a final ACM Transactions on Algorithms PDF and separate PDF and source uploads through Blackboard.
