#include "course_fixture.hpp"
#include <chrono>
#include <iomanip>

int main() {
    return course_test::run([] {
        using course_test::require;
        using Clock = std::chrono::steady_clock;
        constexpr int size = apsp::OpenFlightsLoader::expanded_size;
        const auto graph = apsp::OpenFlightsLoader::load(
            DATASET_DIR "/datos_aeropuertos_validacion.dat",
            DATASET_DIR "/datos_rutas_validacion.dat",
            DATASET_DIR "/aeropuertos_demo.txt", size, true);
        require(graph.airports.size() == size && graph.edges.size() == 20001,
                "The historical expansion should produce 500 airports and 20,001 directed edges");
        const int source = graph.find("MEX"), target = graph.find("NRT");
        std::vector<double> milliseconds;
        for (int trial = 0; trial <= 5; ++trial) {
            const auto start = Clock::now();
            const apsp::FloydWarshall routes(size, graph.edges);
            const auto stop = Clock::now();
            require(!routes.path(source, target).empty() && routes.distance(source, target) > 0 &&
                    !routes.path(target, source).empty() && routes.distance(target, source) > 0,
                    "Both validation queries should be reachable");
            if (trial > 0) {
                milliseconds.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
            }
        }
        std::sort(milliseconds.begin(), milliseconds.end());
        std::cout << "OpenFlights expanded: " << size << " airports, " << graph.edges.size()
                  << " directed edges, Floyd-Warshall constructor (5 trials, ms): median "
                  << std::fixed << std::setprecision(3) << milliseconds[milliseconds.size() / 2]
                  << ", min " << milliseconds.front() << ", max " << milliseconds.back() << '\n';
    });
}
