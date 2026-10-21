#include "openflights_loader.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

// JSON es solo el formato de intercambio con la ventana Python.
static std::string quoted(const std::string& text) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}

static std::string graph_json(const apsp::Graph& graph) {
    std::ostringstream out;
    out << std::setprecision(12) << "\"nodes\":[";
    for (std::size_t i = 0; i < graph.airports.size(); ++i) {
        const auto& a = graph.airports[i];
        out << (i ? "," : "") << "{\"code\":" << quoted(a.code) << ",\"name\":" << quoted(a.name)
            << ",\"city\":" << quoted(a.city) << ",\"country\":" << quoted(a.country)
            << ",\"lat\":" << a.latitude << ",\"lon\":" << a.longitude << '}';
    }
    out << "],\"edges\":[";
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& e = graph.edges[i];
        out << (i ? "," : "") << '[' << quoted(graph.airports[e.from].code) << ','
            << quoted(graph.airports[e.to].code) << ',' << e.km << ']';
    }
    return out.str() + ']';
}

static std::string route_json(const apsp::Graph& graph, const apsp::FloydWarshall& solver,
                              const std::string& source, const std::string& target) {
    int from = graph.find(source), to = graph.find(target);
    auto path = solver.path(from, to);
    std::ostringstream out;
    out << std::setprecision(12) << "\"source\":" << quoted(source) << ",\"target\":" << quoted(target)
        << ",\"distance_km\":";
    if (path.empty()) out << "null";
    else out << solver.distance(from, to);
    out << ",\"path\":[";
    for (std::size_t i = 0; i < path.size(); ++i)
        out << (i ? "," : "") << quoted(graph.airports[path[i]].code);
    return out.str() + ']';
}

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> args{
            {"--airports", "dataset/datos_aeropuertos_validacion.dat"},
            {"--routes", "dataset/datos_rutas_validacion.dat"},
            {"--selection", "dataset/aeropuertos_demo.txt"},
            {"--from", "MEX"}, {"--to", "NRT"}, {"--limit", "150"}};
        bool serve = false;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--serve") { serve = true; continue; }
            if (key == "--help") {
                std::cout << "apsp_demo [--from MEX] [--to NRT] [--limit 150]\n"
                    << "Usa los primeros N codigos de dataset/aeropuertos_demo.txt.\n"
                    << "--serve mantiene las matrices y recibe pares IATA por la entrada estandar.\n"
                    << "Archivos alternativos: --airports, --routes, --selection.\n";
                return 0;
            }
            if (!args.count(key) || i + 1 >= argc)
                throw std::invalid_argument("Argumento invalido: " + key);
            args[key] = argv[++i];
        }
        auto graph = apsp::OpenFlightsLoader::load(args["--airports"], args["--routes"],
            args["--selection"], apsp::OpenFlightsLoader::integer(args["--limit"]));
        // Se ejecuta una sola vez. Las consultas siguientes reutilizan D y Pi.
        apsp::FloydWarshall solver(static_cast<int>(graph.airports.size()), graph.edges);
        if (!serve) {
            auto route = route_json(graph, solver, args["--from"], args["--to"]);
            std::cout << '{' << graph_json(graph) << ',' << route << "}\n";
            return 0;
        }
        std::cout << '{' << graph_json(graph) << '}' << std::endl;
        std::string source, target;
        while (std::cin >> source >> target) {
            try {
                auto route = route_json(graph, solver, source, target);
                std::cout << '{' << route << '}' << std::endl;
            } catch (const std::invalid_argument& error) {
                std::cout << "{\"error\":" << quoted(error.what()) << '}' << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
