# Demo de rutas aereas con Floyd-Warshall

Este programa representa aeropuertos como nodos y vuelos directos como conexiones dirigidas. Calcula los caminos de menor distancia acumulada entre todos los pares de aeropuertos seleccionados. Permite consultar esos caminos y verlos en una ventana.

El algoritmo y la reconstruccion de rutas estan en C++17. Python solo presenta los resultados. La demo usa datos reales e historicos de OpenFlights. Minimiza kilometros, no precios, tiempo de viaje ni numero de vuelos.

## Abrir la demo

(Para Windows, si ya tienes el equipo necesario)
Haz doble clic en `iniciar_demo.cmd` o ejecuta desde esta carpeta:

```powershell
.\iniciar_demo.cmd
```

El iniciador compila con CMake y abre la ventana. Los datos ya estan incluidos en `dataset`; no se necesita internet. La interfaz usa Tkinter. No necesita instalar paquetes con pip.

La ventana tiene dos pestanas:

- **Esquema de la ruta:** puntos grises para los aeropuertos incluidos y lineas verdes para el recorrido consultado. La casilla de conexiones permite ver tambien el resto del grafo.
- **Aeropuertos incluidos:** lista completa con numero, IATA, ciudad, pais y nombre. Los selectores de origen y destino solo ofrecen aeropuertos de esta lista.

Las lineas son un esquema de longitud y latitud, no un mapa con fronteras ni trayectorias reales. Al cruzar el meridiano 180, una conexion se divide entre los bordes del dibujo.

## Que 150 aeropuertos estamos usando

Son exactamente los codigos escritos en `dataset/aeropuertos_demo.txt`. La lista es fija: consultar otro origen o destino no cambia el grafo.

Para preparar esta lista se conservaron MEX y NRT como los dos primeros, y se completaron los otros 148 lugares con aeropuertos de mayor numero de conexiones dirigidas distintas en los archivos locales: entradas mas salidas, con desempate por ID OpenFlights. No es un ranking de pasajeros ni de trafico actual. Ese orden ya quedo escrito en el archivo y no se recalcula al usar la demo.

Los primeros diez son MEX, NRT, FRA, CDG, AMS, ISL, ATL, ORD, PEK y MUC. Puedes consultar los 150 nombres en la segunda pestana o ejecutar:

```powershell
.\iniciar_demo.cmd --list
```

El lector conserva un vuelo solamente si sus dos aeropuertos estan en esa lista. Eso forma un **subgrafo** de OpenFlights: una parte de la red original. Con los archivos actuales, la seleccion predeterminada tiene **150 aeropuertos y 6552 conexiones dirigidas**.

Puedes editar los codigos, uno por linea, y volver a abrir la demo. Los codigos deben existir en el archivo de aeropuertos y no estar repetidos. Los datos originales `.dat` no se modifican.

## Que significa el limite

`--limit N` toma los primeros N aeropuertos del archivo de seleccion. Cuenta origen, destino y posibles intermediarios; no limita el numero de vuelos de una ruta. El limite predeterminado es 150 porque la lista actual contiene 150 codigos. Para usar mas, primero hay que ampliar la lista. La demo admite hasta 1000.

Por ejemplo:

```powershell
.\iniciar_demo.cmd --limit 2
```

Carga solamente MEX y NRT, porque son las dos primeras lineas de la lista. MEX -> NRT no tiene conexion directa en estos datos y no puede usar LAX, por lo que no existe ruta en ese grafo. NRT -> MEX si tiene conexion directa. Al cerrar y abrir con el limite predeterminado, LAX vuelve a estar disponible.

Un aeropuerto fuera de la lista, como GKA en esta version, **no esta incluido**; eso es distinto de que dos aeropuertos incluidos no tengan un camino. La demo ahora hace esa distincion y no introduce automaticamente aeropuertos al consultar.

## Como funciona el programa

```text
Dos archivos OpenFlights + lista del equipo
                 |
                 v
         Grafo dirigido en C++
                 |
                 v
      Floyd-Warshall una sola vez
        matrices de distancias D
        y de predecesores Pi
                 |
                 v
  Consultas de origen y destino en C++
        reconstruccion con Pi
                 |
                 v
     Resultado y dibujo en Python
```

1. El lector obtiene las coordenadas de los aeropuertos elegidos y las conexiones entre ellos.
2. Calcula cada peso con Haversine, usando un radio terrestre de 6371 km.
3. Floyd-Warshall inicializa una matriz D: cero en la diagonal, peso del vuelo cuando hay conexion directa e infinito en los otros casos.
4. Sus tres ciclos prueban si pasar por cada aeropuerto intermedio k mejora D[i][j]. Cuando mejora, tambien se actualiza Pi[i][j].
5. Al elegir origen y destino, C++ consulta D y sigue Pi para recuperar los aeropuertos del camino. Python no vuelve a calcular la ruta.

C++ permanece abierto como un proceso local mientras la ventana este abierta. Al iniciar envia el grafo a Python; despues recibe pares IATA y devuelve rutas. Usa lineas JSON para intercambiar datos. Al cerrar la ventana, el proceso termina.

## Casos comprobados

Estos resultados corresponden a la lista y a los archivos actuales, con 150 nodos:

| Consulta | Camino | Distancia aproximada |
| --- | --- | --- |
| MEX -> LAX | MEX -> LAX | 2500.00 km |
| MEX -> NRT | MEX -> LAX -> NRT | 11253.82 km |
| NRT -> MEX | NRT -> MEX | 11253.73 km |
| MEX -> LHR | MEX -> LHR | 8903.86 km |
| TFS -> HER | TFS -> LPA -> CMN -> TUN -> MLA -> ATH -> HER | 4318.10 km |
| MEX -> MEX | MEX | 0 km |

Una ruta de seis vuelos tiene siete aeropuertos y cinco escalas intermedias. La ruta elegida minimiza kilometros acumulados dentro del grafo seleccionado. No se afirma que sea el mejor itinerario comercial ni el optimo de toda la red global.

Consulta de terminal sin ventana:

```powershell
.\iniciar_demo.cmd --headless --from TFS --to HER
```

## Compilar y probar manualmente

Se necesitan CMake 3.20+, un compilador C++17 y Python 3.10+ con Tkinter. En Ubuntu/Debian se pueden instalar con `sudo apt install build-essential cmake python3 python3-tk`.

En Linux, desde esta carpeta:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s test -p "test_*.py" -v
python3 scripts/demo.py
```

En Windows con MinGW/Ninja, para una carpeta build nueva:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Para las pruebas Python usa el mismo interprete que la demo. En esta computadora, si no tienes `python` o `py` configurado:

```powershell
& "$env:USERPROFILE\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" -m unittest discover -s test -p "test_*.py" -v
```

El ejecutable C++ tambien funciona sin Python: `build\bin\apsp_demo.exe --from MEX --to NRT` en Windows o `./build/bin/apsp_demo --from MEX --to NRT` en Linux. En Visual Studio puede estar en `build/bin/Release`. Su salida es JSON y se debe ejecutar desde la raiz del proyecto para encontrar dataset.

Esta version se comprobo con MinGW en Windows. CMake y el codigo son compatibles con Linux, pero no se ha ejecutado una comprobacion en Linux. Pasaron la prueba C++, diez pruebas Python y la comprobacion automatica de inicializacion y dibujo de la ventana.