#pragma once

#include "openflights_loader.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace apsp {

inline constexpr double detour_tolerance_km = 1e-6;

struct RemovalImpact {
    int airport;
    int baseline_reachable = 0;
    int lost = 0;
    int still_reachable = 0;
    int detoured = 0;
    double total_detour_km = 0;
    double max_detour_km = 0;
};

struct RemovalScenario {
    Graph graph;
    std::optional<FloydWarshall> solver;
    RemovalImpact impact;
};

inline RemovalScenario make_removal_scenario(const Graph& graph, const FloydWarshall& baseline,
                                             int removed) {
    const int n = static_cast<int>(graph.airports.size());
    if (removed < 0 || removed >= n) throw std::out_of_range("Airport index outside graph");

    RemovalScenario scenario{{}, std::nullopt, {removed}};
    if (n == 1) return scenario;

    // Preserve relative vertex order when compacting the surviving graph.
    const auto survivor_index = [removed](int vertex) { return vertex - (vertex > removed); };
    scenario.graph.airports.reserve(n - 1);
    for (int vertex = 0; vertex < n; ++vertex) {
        if (vertex != removed) scenario.graph.airports.push_back(graph.airports[vertex]);
    }
    scenario.graph.edges.reserve(graph.edges.size());
    for (const auto& edge : graph.edges) {
        if (edge.from == removed || edge.to == removed) continue;
        scenario.graph.edges.push_back({survivor_index(edge.from), survivor_index(edge.to), edge.km});
    }

    const auto& reduced = scenario.solver.emplace(n - 1, scenario.graph.edges);
    auto& impact = scenario.impact;
    for (int source = 0; source < n; ++source) {
        if (source == removed) continue;
        for (int target = 0; target < n; ++target) {
            if (target == removed || target == source) continue;
            const double before = baseline.distance(source, target);
            const double after = reduced.distance(survivor_index(source), survivor_index(target));
            if (!std::isfinite(before)) {
                if (std::isfinite(after)) throw std::logic_error("Deletion created reachability");
                continue;
            }
            ++impact.baseline_reachable;
            if (!std::isfinite(after)) {
                ++impact.lost;
                continue;
            }
            ++impact.still_reachable;
            const double increase = after - before;
            if (increase < -detour_tolerance_km) throw std::logic_error("Deletion shortened a route");
            if (increase <= detour_tolerance_km) continue;
            ++impact.detoured;
            impact.total_detour_km += increase;
            impact.max_detour_km = std::max(impact.max_detour_km, increase);
        }
    }
    return scenario;
}

inline RemovalImpact remove_airport(const Graph& graph, const FloydWarshall& baseline, int removed) {
    return make_removal_scenario(graph, baseline, removed).impact;
}

}  // namespace apsp
