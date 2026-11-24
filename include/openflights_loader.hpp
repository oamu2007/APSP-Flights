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
    static constexpr int historical_size = 150;
    static constexpr int expanded_size = 500;

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
                      const std::string& selection_file, int limit = historical_size,
                      bool expand = false) {
        if (expand && (limit < 1 || limit > expanded_size))
            throw std::invalid_argument("The expanded selection supports 1 to 500 airports");
        if (!expand && (limit < 1 || limit > 1000))
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

        // Keep the selection file as the prefix, with its original order.
        Graph graph;
        std::map<int, int> selected_ids;
        std::set<std::string> selected_codes;
        const auto select = [&](const Airport& airport) {
            selected_ids[airport.id] = static_cast<int>(graph.airports.size());
            graph.airports.push_back(airport);
        };
        while (std::getline(selection, line)) {
            std::string code = line.substr(0, line.find('#'));
            code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char c) {
                return c == ' ' || c == '\t' || c == '\r';
            }), code.end());
            if (code.empty()) continue;
            if (!all.count(code)) throw std::invalid_argument("Codigo no encontrado en OpenFlights: " + code);
            if (!selected_codes.insert(code).second)
                throw std::invalid_argument("Codigo duplicado en aeropuertos_demo.txt: " + code);
            if (graph.airports.size() < static_cast<std::size_t>(limit)) select(all.at(code));
        }
        if (!expand && graph.airports.size() != static_cast<std::size_t>(limit))
            throw std::invalid_argument("La lista contiene menos aeropuertos que el limite solicitado");

        // Expansion ranks airports by their degree in the full usable network.
        // For a fixed selection, only its induced edges are needed.
        const bool needs_ranking = graph.airports.size() < static_cast<std::size_t>(limit);
        std::set<std::pair<int, int>> connections;
        while (std::getline(route_input, line)) {
            try {
                auto f = csv(line);
                if (f.size() < 9 || integer(f[7]) != 0) continue;
                const int source = integer(f[3]), target = integer(f[5]);
                if (source == target) continue;
                if (needs_ranking) {
                    if (ids.count(source) && ids.count(target)) connections.emplace(source, target);
                } else if (selected_ids.count(source) && selected_ids.count(target)) {
                    connections.emplace(source, target);
                }
            } catch (const std::invalid_argument&) { /* ID ausente o fila invalida. */ }
              catch (const std::out_of_range&) { /* Numero fuera de rango. */ }
        }
        if (needs_ranking) {
            std::map<int, int> degrees;
            for (const auto& [source, target] : connections) {
                ++degrees[source];
                ++degrees[target];
            }
            std::map<std::pair<int, int>, const Airport*> ranked;
            for (const auto& [code, airport] : all) {
                const auto degree = degrees.find(airport.id);
                if (!selected_codes.count(code) && degree != degrees.end())
                    ranked.emplace(std::make_pair(-degree->second, airport.id), &airport);
            }
            for (const auto& entry : ranked) {
                select(*entry.second);
                if (graph.airports.size() == static_cast<std::size_t>(limit)) break;
            }
            if (graph.airports.size() != static_cast<std::size_t>(limit))
                throw std::invalid_argument("Not enough connected airports to expand the selection");
        }
        for (const auto& [source_id, target_id] : connections) {
            const auto source_index = selected_ids.find(source_id), target_index = selected_ids.find(target_id);
            if (source_index == selected_ids.end() || target_index == selected_ids.end()) continue;
            const int source = source_index->second, target = target_index->second;
            double km = haversine(graph.airports[source], graph.airports[target]);
            graph.edges.push_back({source, target, km});
        }
        std::sort(graph.edges.begin(), graph.edges.end(), [](const Edge& a, const Edge& b) {
            return std::make_pair(a.from, a.to) < std::make_pair(b.from, b.to);
        });
        return graph;
    }
};
}
