#include "course_fixture.hpp"

int main() {
    return course_test::run([] {
        using course_test::require;
        const auto graph = course_test::small_graph();
        require(graph.airports.size() == 5, "The selection should insert five airports");
        require(graph.airports.front().name == "Alpha, Main", "A quoted airport name should retain its comma");
        const std::vector<std::pair<int, int>> expected{{0, 1}, {0, 3}, {1, 2}, {3, 2}, {4, 1}};
        std::vector<std::pair<int, int>> inserted;
        for (const auto& edge : graph.edges) {
            inserted.emplace_back(edge.from, edge.to);
            require(edge.km > 0 && std::isfinite(edge.km), "Direct flight weights should be finite and positive");
        }
        require(inserted == expected, "Expected only the five valid, distinct directed connections");
        const apsp::FloydWarshall routes(5, graph.edges);
        require(routes.path(0, 1) == std::vector<int>({0, 1}), "AAA to BBB should enter the route matrix directly");
    });
}
