# Datos de la demo

Los dos archivos .dat son los datos reales que se tomaron de el repositorio OpenFlights el 20 de octubre de 2026. Son CSV codificados en UTF-8 aunque tengan extension .dat.

| Archivo local | Archivo original | Contenido utilizado |
| --- | --- | --- |
| `datos_aeropuertos_validacion.dat` | [airports.dat](https://raw.githubusercontent.com/jpatokal/openflights/master/data/airports.dat) | ID, nombre, ciudad, pais, IATA, latitud y longitud. |
| `datos_rutas_validacion.dat` | [routes.dat](https://raw.githubusercontent.com/jpatokal/openflights/master/data/routes.dat) | ID de aeropuerto origen, ID de destino y numero de paradas. |
| `aeropuertos_demo.txt` | Lista preparada para este proyecto | Seleccion fija de 150 codigos IATA. El orden se explica en el README principal. |

El lector descarta aeropuertos sin IATA o coordenadas validas, conserva la primera aparicion de IDs/codigos duplicados y rechaza codigos desconocidos o repetidos en la lista de seleccion. De routes.dat conserva solo registros con cero paradas y IDs que correspondan a dos aeropuertos seleccionados. Excluye autoaristas y registros sin IDs validos. Colapsa conexiones repetidas entre el mismo par dirigido porque todas se ponderan con la misma distancia geografica. No agrega conexiones inversas que no esten en los datos.

Las rutas son historicas: segun la [documentacion de OpenFlights](https://openflights.org/data.php), su proveedor dejo de actualizarlas en junio de 2014. No deben interpretarse como vuelos o disponibilidad comercial actuales.

OpenFlights publica estas bases bajo [Open Database License](https://opendatacommons.org/licenses/odbl/1-0/) y sus contenidos individuales bajo [Database Contents License](https://opendatacommons.org/licenses/dbcl/1-0/), segun su pagina de datos. Conserva la atribucion a OpenFlights y revisa esas condiciones al redistribuir bases o derivados. Esto documenta las condiciones de los datos; no asigna una licencia al codigo nuevo del proyecto.

Huellas SHA256 de los archivos locales usados para las pruebas:

```text
datos_aeropuertos_validacion.dat
9387cdb38df5bd664da823f8ccb69fdd9b33a1888f5b7cca09c34a3cd9ff59f9

datos_rutas_validacion.dat
bd373706238134f619c624c606dccc74c05c2582a977c489c81de501735f2390
```

Las huellas identifican el contenido local, no garantizan por si solas su autenticidad. Si se cambian los archivos o la lista de seleccion, deben volver a comprobarse las rutas del README y las pruebas con respuestas conocidas.

## Expanded airport selection

The application defaults to the fixed 150 codes in `aeropuertos_demo.txt`. The expanded preset retains that prefix in file order. It ranks remaining airports with at least one connection by descending in-degree plus out-degree in the full usable OpenFlights network, breaking ties by ascending airport ID. The degree calculation counts distinct zero-stop directed pairs between validated airports and excludes self-edges. The selected graph then keeps only connections with both endpoints in the selected prefix. The larger graph still favors well-connected airports.

Use `--preset expanded` for 500 airports or add `--limit N` to choose its first 1–500 airports. The historical evaluation tools continue to use the fixed 150 codes. The integration tests independently reconstruct the expanded order and directed edge set from the two bundled data files.

| Preset | Airports | Directed edges | Median startup (min–max) | Maximum backend peak working set | Matrix entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| Historical | 150 | 6,552 | 233.6 ms (195.3–249.4) | 7.12 MiB | 0.26 MiB |
| Expanded prefix | 250 | 11,440 | 330.8 ms (303.2–453.9) | 8.93 MiB | 0.72 MiB |
| Expanded | 500 | 20,001 | 373.6 ms (345.6–385.1) | 10.68 MiB | 2.86 MiB |

On Windows x64 with an Intel Core i7-1255U and MinGW-w64 GCC 15.2.0, CMake Release (`-O3 -DNDEBUG`), startup was measured from Python starting a new backend to receiving and decoding its initial graph. For each size, one warmup preceded five measured launches. The Windows peak working-set counter covers the C++ process. The matrix column estimates the payload of the two matrices at 12 bytes per vertex pair, before container overhead. The expanded 500-airport configuration stayed below 0.4 seconds and 11 MiB in these samples, so 500 is the supported expanded cap. The Tkinter window loads and switches selections in the background and provides an airport filter by code, city, country or name.
