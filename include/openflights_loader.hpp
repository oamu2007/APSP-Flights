#pragma once
#include "floyd_warshall.hpp"
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace apsp {

struct Airport {
    int id;
    std::string code, name, city, country;
    double latitude, longitude;
};

struct Graph {
    std::vector<Airport> airports;
    std::vector<Edge> edges;

    int find(const std::string& code) const {
        for (std::size_t i = 0; i < airports.size(); ++i)
            if (airports[i].code == code) return static_cast<int>(i);
        throw std::invalid_argument("Aeropuerto fuera de la lista seleccionada: " + code);
    }
};

class OpenFlightsLoader {
public:
    // OpenFlights usa CSV aunque sus archivos terminen en .dat.
    static std::vector<std::string> csv(const std::string& line) {
        std::vector<std::string> fields;
        std::string value;
        bool quoted = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                    value += c;
                    ++i;
                } else quoted = !quoted;
            } else if (c == ',' && !quoted) {
                fields.push_back(value);
                value.clear();
            } else if (c != '\r') value += c;
        }
        if (quoted) throw std::invalid_argument("CSV con comillas sin cerrar");
        fields.push_back(value);
        return fields;
    }

    static int integer(const std::string& text) {
        std::size_t used;
        int value = std::stoi(text, &used);
        if (used != text.size()) throw std::invalid_argument("Entero invalido");
        return value;
    }

    static double number(const std::string& text) {
        std::size_t used;
        double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value))
            throw std::invalid_argument("Numero invalido");
        return value;
    }

    static double haversine(const Airport& a, const Airport& b) {
        const double radians = std::acos(-1.0) / 180.0;
        double dlat = (b.latitude - a.latitude) * radians;
        double dlon = (b.longitude - a.longitude) * radians;
        double h = std::pow(std::sin(dlat / 2), 2) +
            std::cos(a.latitude * radians) * std::cos(b.latitude * radians) *
            std::pow(std::sin(dlon / 2), 2);
        return 2 * 6371.0 * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
    }

    static Graph load(const std::string& airport_file, const std::string& route_file,
                      const std::string& selection_file, int limit = 150) {
        if (limit < 1 || limit > 1000)
            throw std::invalid_argument("Limite permitido: 1 a 1000");
        std::ifstream airport_input(airport_file), route_input(route_file), selection(selection_file);
        if (!airport_input || !route_input || !selection)
            throw std::runtime_error("Falta un archivo en dataset; revisa los dos .dat y aeropuertos_demo.txt");

        // 1. Leer aeropuertos validos de OpenFlights y buscarlos por IATA.
        std::map<std::string, Airport> all;
        std::set<int> ids;
        std::string line;
        while (std::getline(airport_input, line)) {
            try {
                auto f = csv(line);
                if (f.size() < 8 || f[4].size() != 3 || f[4] == "\\N") continue;
                Airport airport{integer(f[0]), f[4], f[1], f[2], f[3], number(f[6]), number(f[7])};
                if (std::abs(airport.latitude) > 90 || std::abs(airport.longitude) > 180 ||
                    all.count(airport.code) || ids.count(airport.id)) continue;
                ids.insert(airport.id);
                all.emplace(airport.code, airport);
            } catch (const std::invalid_argument&) { /* Fila sin datos utilizables. */ }
              catch (const std::out_of_range&) { /* Numero fuera de rango. */ }
        }

        // 2. Elegir exactamente los primeros N codigos de la lista del equipo.
        Graph graph;
        std::map<int, int> selected_ids;
        std::set<std::string> selected_codes;
        while (std::getline(selection, line)) {
            std::string code = line.substr(0, line.find('#'));
            code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char c) {
                return c == ' ' || c == '\t' || c == '\r';
            }), code.end());
            if (code.empty()) continue;
            if (!all.count(code)) throw std::invalid_argument("Codigo no encontrado en OpenFlights: " + code);
            if (!selected_codes.insert(code).second)
                throw std::invalid_argument("Codigo duplicado en aeropuertos_demo.txt: " + code);
            if (graph.airports.size() < static_cast<std::size_t>(limit)) {
                const Airport& airport = all.at(code);
                selected_ids[airport.id] = static_cast<int>(graph.airports.size());
                graph.airports.push_back(airport);
            }
        }
        if (graph.airports.size() != static_cast<std::size_t>(limit))
            throw std::invalid_argument("La lista contiene menos aeropuertos que el limite solicitado");

        // 3. Conservar vuelos directos cuyos dos extremos estan seleccionados.
        std::set<std::pair<int, int>> connections;
        while (std::getline(route_input, line)) {
            try {
                auto f = csv(line);
                if (f.size() < 9 || integer(f[7]) != 0) continue;
                auto source = selected_ids.find(integer(f[3]));
                auto target = selected_ids.find(integer(f[5]));
                if (source == selected_ids.end() || target == selected_ids.end() || source == target) continue;
                // Varias aerolineas entre el mismo par representan una conexion.
                connections.emplace(source->second, target->second);
            } catch (const std::invalid_argument&) { /* ID ausente o fila invalida. */ }
              catch (const std::out_of_range&) { /* Numero fuera de rango. */ }
        }
        for (auto [source, target] : connections) {
            double km = haversine(graph.airports[source], graph.airports[target]);
            graph.edges.push_back({source, target, km});
        }
        return graph;
    }
};
}
