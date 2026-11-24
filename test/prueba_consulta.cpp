#include "course_fixture.hpp"

int main() {
    return course_test::run([] {
        using course_test::require;
        const auto graph = course_test::small_graph();
        const apsp::FloydWarshall routes(static_cast<int>(graph.airports.size()), graph.edges);
        const int alpha = graph.find("AAA"), bravo = graph.find("BBB"), charlie = graph.find("CCC");
        const double equatorial_km = 6371.0 * std::acos(-1.0) / 90;
        require(routes.path(alpha, charlie) == std::vector<int>({alpha, bravo, charlie}),
                "The shortest AAA to CCC route should use BBB");
        require(std::abs(routes.distance(alpha, charlie) - equatorial_km) < 1e-6,
                "The path spans two degrees along the equator");
        require(routes.path(charlie, alpha).empty() && std::isinf(routes.distance(charlie, alpha)),
                "The reverse path should be unreachable");
        require(routes.path(charlie, charlie) == std::vector<int>({charlie}) &&
                routes.distance(charlie, charlie) == 0, "An identity route should have zero distance");
    });
}
