#include "airport_removal.hpp"
#include "course_fixture.hpp"

int main() {
    return course_test::run([] {
        using course_test::require;
        const auto baseline = course_test::small_graph();
        const apsp::FloydWarshall routes(static_cast<int>(baseline.airports.size()), baseline.edges);
        const int removed = baseline.find("BBB");
        const auto scenario = apsp::make_removal_scenario(baseline, routes, removed);
        require(scenario.graph.airports.size() == 4 && scenario.graph.edges.size() == 2 && scenario.solver,
                "Deleting BBB should leave four airports and two directed edges");
        const int alpha = scenario.graph.find("AAA"), charlie = scenario.graph.find("CCC");
        const int delta = scenario.graph.find("DDD"), echo = scenario.graph.find("EEE");
        require(scenario.solver->path(alpha, charlie) == std::vector<int>({alpha, delta, charlie}),
                "The surviving AAA to CCC route should use DDD");
        require(scenario.solver->path(echo, charlie).empty(), "EEE loses its route to CCC");
        const auto& impact = scenario.impact;
        require(impact.baseline_reachable == 4 && impact.lost == 1 &&
                impact.still_reachable == 3 && impact.detoured == 1, "Wrong surviving pair counts");
        const double increase = scenario.solver->distance(alpha, charlie) -
            routes.distance(baseline.find("AAA"), baseline.find("CCC"));
        require(increase > 0 && std::abs(impact.total_detour_km - increase) < 1e-6 &&
                std::abs(impact.max_detour_km - increase) < 1e-6, "Wrong detour magnitude");
        require(routes.path(baseline.find("AAA"), baseline.find("CCC")) == std::vector<int>({0, removed, 2}),
                "The baseline must still provide its original route");
    });
}
