#include "openflights_loader.hpp"
#include <iostream>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    try {
        // Grafo pequeno con respuesta conocida: 0 -> 1 -> 2 -> 3 pesa 9.
        apsp::FloydWarshall solver(5, {{0,1,3}, {1,2,4}, {0,2,20}, {2,3,2}, {0,1,8}});
        check(solver.path(0,3) == std::vector<int>({0,1,2,3}), "Reconstruccion incorrecta");
        check(solver.distance(0,3) == 9, "Distancia incorrecta");
        check(solver.path(3,0).empty(), "Debe respetar la direccion");
        check(solver.path(0,4).empty(), "Nodo aislado debe ser inalcanzable");
        check(solver.path(4,4) == std::vector<int>({4}), "Ruta al mismo nodo");
        apsp::FloydWarshall zero(3, {{0,1,0}, {1,0,0}, {1,2,2}});
        check(zero.path(0,2) == std::vector<int>({0,1,2}), "Peso cero");
        bool rejected = false;
        try { apsp::FloydWarshall bad(2, {{0,1,-1}}); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Se deben rechazar pesos negativos");

        auto fields = apsp::OpenFlightsLoader::csv("1,\"Airport, \"\"Name\"\"\",X");
        check(fields.size() == 3 && fields[1] == "Airport, \"Name\"", "Lectura CSV");
        apsp::Airport a{1,"AAA","","","",0,0}, b{2,"BBB","","","",0,90};
        double expected = 6371 * std::acos(-1.0) / 2;
        check(std::abs(apsp::OpenFlightsLoader::haversine(a,b)-expected) < 1e-6, "Haversine");

        std::string folder = DATASET_DIR;
        auto graph = apsp::OpenFlightsLoader::load(folder+"/datos_aeropuertos_validacion.dat",
            folder+"/datos_rutas_validacion.dat", folder+"/aeropuertos_demo.txt");
        apsp::FloydWarshall real(static_cast<int>(graph.airports.size()), graph.edges);
        auto path = real.path(graph.find("MEX"), graph.find("NRT"));
        check(graph.airports.size() == 150, "Lista de 150 aeropuertos");
        check(path.size() == 3 && graph.airports[path[1]].code == "LAX", "Ruta real por LAX");
        check(real.path(graph.find("NRT"), graph.find("MEX")).size() == 2, "Regreso directo");
        std::cout << "OK: Floyd-Warshall, predecesores, CSV, Haversine y OpenFlights\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
