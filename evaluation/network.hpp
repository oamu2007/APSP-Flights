#pragma once

#include "airport_removal.hpp"
#include <optional>
#include <tuple>

namespace evaluation {

struct AirportDegrees {
    int in = 0;
    int out = 0;
};

struct NetworkProfile {
    std::vector<AirportDegrees> degrees;
    std::vector<int> component_by_airport;
    std::vector<int> component_sizes;
    int reachable_pairs = 0;
    int one_way_pairs = 0;
    int one_way_edges = 0;
};

inline NetworkProfile profile(const apsp::Graph& graph, const apsp::FloydWarshall& solver) {
    const int n = static_cast<int>(graph.airports.size());
    NetworkProfile result;
    result.degrees.resize(n);
    result.component_by_airport.assign(n, -1);

    std::set<std::pair<int, int>> connections;
    for (const auto& edge : graph.edges) {
        ++result.degrees[edge.from].out;
        ++result.degrees[edge.to].in;
        connections.emplace(edge.from, edge.to);
    }
    for (const auto& edge : graph.edges) {
        if (!connections.count({edge.to, edge.from})) ++result.one_way_edges;
    }

    // Mutual reachability partitions the vertices into strongly connected components.
    // Assign IDs in selection order using the first unassigned airport as representative.
    for (int source = 0; source < n; ++source) {
        for (int target = 0; target < n; ++target) {
            if (source == target || !std::isfinite(solver.distance(source, target))) continue;
            ++result.reachable_pairs;
            if (!std::isfinite(solver.distance(target, source))) ++result.one_way_pairs;
        }
        if (result.component_by_airport[source] >= 0) continue;
        const int component = static_cast<int>(result.component_sizes.size());
        int size = 0;
        for (int target = source; target < n; ++target) {
            if (std::isfinite(solver.distance(source, target)) &&
                std::isfinite(solver.distance(target, source))) {
                result.component_by_airport[target] = component;
                ++size;
            }
        }
        result.component_sizes.push_back(size);
    }
    return result;
}

using apsp::detour_tolerance_km;
using apsp::RemovalImpact;
using apsp::remove_airport;

inline bool longer_first(const apsp::Graph& graph, const apsp::Edge& a, const apsp::Edge& b) {
    if (a.km != b.km) return a.km > b.km;
    return std::tie(graph.airports[a.from].code, graph.airports[a.to].code) <
           std::tie(graph.airports[b.from].code, graph.airports[b.to].code);
}

inline std::vector<apsp::Edge> longest_shortest_paths(const apsp::Graph& graph,
                                                       const apsp::FloydWarshall& solver,
                                                       std::size_t count) {
    const int n = static_cast<int>(graph.airports.size());
    std::vector<apsp::Edge> distances;
    for (int source = 0; source < n; ++source) {
        for (int target = 0; target < n; ++target) {
            if (source == target) continue;
            const double distance = solver.distance(source, target);
            if (std::isfinite(distance)) distances.push_back({source, target, distance});
        }
    }
    const auto end = distances.begin() + std::min(count, distances.size());
    std::partial_sort(distances.begin(), end, distances.end(), [&graph](const auto& a, const auto& b) {
        return longer_first(graph, a, b);
    });
    distances.erase(end, distances.end());
    return distances;
}

inline std::optional<apsp::Edge> longest_direct_edge(const apsp::Graph& graph) {
    if (graph.edges.empty()) return std::nullopt;
    return *std::min_element(graph.edges.begin(), graph.edges.end(), [&graph](const auto& a, const auto& b) {
        return longer_first(graph, a, b);
    });
}

}  // namespace evaluation
