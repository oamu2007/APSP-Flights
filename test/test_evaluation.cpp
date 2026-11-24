#include "network.hpp"
#include <iostream>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_equal_cost_paths() {
    const apsp::FloydWarshall solver(5, {{0, 2, 1}, {2, 3, 2}, {0, 1, 1},
                                        {1, 3, 2}, {1, 4, 2}, {0, 4, 3}});
    check(solver.distance(0, 3) == 3, "Equal-cost alternatives have total weight 3");
    check(solver.path(0, 3) == std::vector<int>({0, 1, 3}),
          "The first equal-cost intermediate in vertex order wins");
    check(solver.path(0, 4) == std::vector<int>({0, 4}),
          "An equal-cost alternative preserves a direct edge");
    check(!std::isfinite(solver.distance(3, 0)) && solver.path(3, 0).empty(),
          "An unreachable reverse pair has infinite distance and an empty path");
}

void check_profile_and_removal() {
    apsp::Graph graph;
    graph.airports.resize(5);
    graph.airports[0].code = "AAA";
    graph.airports[1].code = "BBB";
    graph.airports[2].code = "CCC";
    graph.airports[3].code = "DDD";
    graph.airports[4].code = "EEE";
    graph.edges = {{0, 1, 1}, {1, 2, 1}, {0, 2, 5}, {3, 1, 1}, {2, 4, 2}, {4, 2, 2}};
    const apsp::FloydWarshall solver(5, graph.edges);
    const auto profile = evaluation::profile(graph, solver);
    check(profile.component_sizes == std::vector<int>({1, 1, 2, 1}), "Expected four SCCs");
    check(profile.component_by_airport == std::vector<int>({0, 1, 2, 3, 2}), "Wrong SCC membership");
    check(profile.reachable_pairs == 10 && profile.one_way_pairs == 8, "Wrong directed pair counts");
    check(profile.one_way_edges == 4 && profile.degrees[2].in == 3 && profile.degrees[2].out == 1,
          "Wrong directed edge or degree count");

    const auto scenario = apsp::make_removal_scenario(graph, solver, 1);
    check(scenario.graph.airports.size() == 4 && scenario.graph.edges.size() == 3 &&
          scenario.graph.find("AAA") == 0 && scenario.graph.find("CCC") == 1 &&
          scenario.graph.find("DDD") == 2 && scenario.graph.find("EEE") == 3,
          "Deletion preserves surviving airport order and removes incident edges");
    check(scenario.solver && scenario.solver->distance(0, 3) == 7 &&
          scenario.solver->path(0, 3) == std::vector<int>({0, 1, 3}),
          "The scenario solver reconstructs the longer directed route using survivor indices");
    check(scenario.solver->path(2, 1).empty() && scenario.solver->path(3, 0).empty(),
          "Deletion loses the route from DDD to CCC and keeps the reverse route from EEE to AAA unreachable");

    const auto& impact = scenario.impact;
    check(impact.airport == 1 && impact.baseline_reachable == 6 && impact.lost == 2 &&
          impact.still_reachable == 4,
          "Deletion should lose 3 to 2 and 3 to 4 among six previously reachable surviving pairs");
    check(impact.detoured == 2 && impact.total_detour_km == 6 && impact.max_detour_km == 3,
          "Deletion should add 3 km each to 0 to 2 and 0 to 4");

    apsp::Graph sparse;
    sparse.airports.resize(3);
    sparse.edges = {{0, 1, 1}};
    const apsp::FloydWarshall sparse_solver(3, sparse.edges);
    const auto sparse_scenario = apsp::make_removal_scenario(sparse, sparse_solver, 1);
    check(sparse_scenario.solver && sparse_scenario.graph.edges.empty() &&
          sparse_scenario.solver->path(0, 1).empty() && sparse_scenario.solver->path(1, 0).empty() &&
          sparse_scenario.solver->path(0, 0) == std::vector<int>({0}),
          "The solver distinguishes unreachable survivor pairs from an identity route");
    const auto& empty = sparse_scenario.impact;
    check(empty.baseline_reachable == 0 && empty.lost == 0 && empty.still_reachable == 0 &&
          empty.detoured == 0 && empty.total_detour_km == 0 && empty.max_detour_km == 0,
          "Previously unreachable pairs do not enter the deletion comparison");
}

void check_singleton_removal() {
    apsp::Graph singleton;
    singleton.airports.resize(1);
    const apsp::FloydWarshall solver(1, singleton.edges);
    const auto scenario = apsp::make_removal_scenario(singleton, solver, 0);
    check(scenario.graph.airports.empty() && scenario.graph.edges.empty() && !scenario.solver,
          "Deleting the only airport yields an empty graph and no solver");
    const auto impact = evaluation::remove_airport(singleton, solver, 0);
    check(impact.airport == 0 && impact.baseline_reachable == 0 && impact.lost == 0 &&
          impact.still_reachable == 0 && impact.detoured == 0 && impact.total_detour_km == 0 &&
          impact.max_detour_km == 0, "Removing the only airport leaves no surviving pairs to compare");

    for (const int invalid_index : {-1, 1}) {
        bool rejected = false;
        try {
            apsp::make_removal_scenario(singleton, solver, invalid_index);
        } catch (const std::out_of_range&) {
            rejected = true;
        }
        check(rejected, "Deletion rejects airport indices outside the graph");
    }
}

void check_longest_routes() {
    apsp::Graph graph;
    graph.airports.resize(4);
    graph.airports[0].code = "ZZZ";
    graph.airports[1].code = "AAA";
    graph.airports[2].code = "BBB";
    graph.airports[3].code = "CCC";
    graph.edges = {{0, 1, 4}, {2, 1, 4}, {1, 2, 4}};
    const apsp::FloydWarshall solver(4, graph.edges);
    const auto paths = evaluation::longest_shortest_paths(graph, solver, 10);
    check(paths.size() == 4, "Ranking must exclude self and unreachable pairs");
    check(paths[0].from == 0 && paths[0].to == 2 && paths[0].km == 8 &&
          solver.path(paths[0].from, paths[0].to) == std::vector<int>({0, 1, 2}),
          "Longest shortest path should take two hops");
    check(paths[1].from == 1 && paths[1].to == 2 && paths[2].from == 2 && paths[2].to == 1 &&
          paths[3].from == 0 && paths[3].to == 1, "Equal distances sort by IATA endpoints");
    check(evaluation::longest_shortest_paths(graph, solver, 2).size() == 2, "Ranking respects its limit");
    const auto edge = evaluation::longest_direct_edge(graph);
    check(edge && edge->from == 1 && edge->to == 2 && edge->km == 4, "Longest direct edge uses IATA tie rule");
}

}  // namespace

int main() {
    try {
        check_equal_cost_paths();
        check_profile_and_removal();
        check_singleton_removal();
        check_longest_routes();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
