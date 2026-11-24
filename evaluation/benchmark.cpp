#include "floyd_warshall.hpp"
#include "openflights_loader.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int warmups = 2;
constexpr int trials = 9;
constexpr int historical_nodes = 150;

struct BenchmarkGraph {
    const char* name;
    int nodes;
    std::vector<apsp::Edge> edges;

    double density() const {
        return static_cast<double>(edges.size()) / (nodes * (nodes - 1));
    }
};

// Use rejection sampling on the standardized MT19937 output so that the
// same seed and graph size yield the same input across standard libraries.
std::uint32_t uniform_below(std::mt19937& engine, std::uint32_t bound) {
    constexpr std::uint64_t outcomes = std::uint64_t{1} << 32;
    const auto cutoff = outcomes - outcomes % bound;
    std::uint64_t draw;
    do {
        draw = engine();
    } while (draw >= cutoff);
    return static_cast<std::uint32_t>(draw % bound);
}

BenchmarkGraph synthetic_graph(int nodes, double density) {
    std::mt19937 engine(20261124u + nodes);
    std::uint32_t remaining_pairs = nodes * (nodes - 1);
    auto remaining_edges = static_cast<std::uint32_t>(std::llround(density * remaining_pairs));
    BenchmarkGraph graph{"synthetic", nodes, {}};
    graph.edges.reserve(remaining_edges);

    // Visit ordered pairs in vertex order. Select each with probability
    // remaining_edges / remaining_pairs to draw an exact-size uniform sample.
    for (int from = 0; from < nodes; ++from) {
        for (int to = 0; to < nodes; ++to) {
            if (from == to) continue;
            if (uniform_below(engine, remaining_pairs) < remaining_edges) {
                // Draw an independent integer weight in [1, 10000] synthetic km.
                const double weight = 1 + uniform_below(engine, 10000);
                graph.edges.push_back({from, to, weight});
                --remaining_edges;
            }
            --remaining_pairs;
        }
    }
    return graph;
}

double consume_result(const apsp::FloydWarshall& result, int nodes) {
    double checksum = 0;
    for (int from = 0; from < nodes; ++from) {
        for (int to = 0; to < nodes; ++to) {
            const double distance = result.distance(from, to);
            if (std::isfinite(distance)) checksum += distance;
        }
        // Also observe predecessor data through the normal reconstruction API.
        for (int vertex : result.path(from, (from + nodes / 2) % nodes)) {
            checksum += vertex + 1;
        }
    }
    return checksum;
}

std::vector<double> measure(const BenchmarkGraph& graph, double& checksum) {
    std::vector<double> samples;
    samples.reserve(trials);
    for (int trial = -warmups; trial < trials; ++trial) {
        const auto start = std::chrono::steady_clock::now();
        const apsp::FloydWarshall result(graph.nodes, graph.edges);
        const auto stop = std::chrono::steady_clock::now();

        // Observe the computed matrices after the clock stops. Destruction also
        // takes place outside the interval, at the end of the loop iteration.
        checksum += consume_result(result, graph.nodes);
        if (trial >= 0) {
            samples.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        }
    }
    return samples;
}

std::ofstream csv_output(const std::filesystem::path& path, const char* header) {
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path);
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(9) << header << '\n';
    return output;
}

void write_results(const std::filesystem::path& directory, const std::vector<BenchmarkGraph>& graphs,
                   const std::vector<std::vector<double>>& samples_by_graph) {
    auto trial_csv = csv_output(directory / "benchmark_trials.csv",
                                "graph,nodes,edges,density,trial,runtime_ms");
    auto summary_csv = csv_output(directory / "benchmark_summary.csv",
                                  "graph,nodes,edges,density,trials,median_ms,min_ms,max_ms,mean_ms,stddev_ms");
    for (std::size_t index = 0; index < graphs.size(); ++index) {
        const auto& graph = graphs[index];
        const auto& samples = samples_by_graph[index];
        for (std::size_t trial = 0; trial < samples.size(); ++trial) {
            trial_csv << graph.name << ',' << graph.nodes << ',' << graph.edges.size() << ','
                      << graph.density() << ',' << trial + 1 << ',' << samples[trial] << '\n';
        }

        auto ordered = samples;
        std::sort(ordered.begin(), ordered.end());
        const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        const double squared_deviations = std::accumulate(samples.begin(), samples.end(), 0.0,
            [mean](double sum, double sample) { return sum + (sample - mean) * (sample - mean); });
        const double sample_stddev = std::sqrt(squared_deviations / (samples.size() - 1));
        const double median = (ordered[(ordered.size() - 1) / 2] + ordered[ordered.size() / 2]) / 2;
        summary_csv << graph.name << ',' << graph.nodes << ',' << graph.edges.size() << ','
                    << graph.density() << ',' << samples.size() << ',' << median << ','
                    << ordered.front() << ',' << ordered.back() << ',' << mean << ',' << sample_stddev << '\n';
    }
    trial_csv.close();
    summary_csv.close();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: apsp_benchmark <output-directory>\n";
        return 2;
    }
    try {
        auto historical = apsp::OpenFlightsLoader::load("dataset/datos_aeropuertos_validacion.dat",
                                                      "dataset/datos_rutas_validacion.dat",
                                                      "dataset/aeropuertos_demo.txt", historical_nodes);
        std::vector<BenchmarkGraph> graphs;
        graphs.push_back({"historical", historical_nodes, std::move(historical.edges)});
        const double target_density = graphs.front().density();
        for (int nodes : {75, 150, 250, 400, 550, 750}) {
            graphs.push_back(synthetic_graph(nodes, target_density));
        }

        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        double checksum = 0;
        std::vector<std::vector<double>> samples_by_graph;
        for (const auto& graph : graphs) {
            samples_by_graph.push_back(measure(graph, checksum));
        }
        write_results(directory, graphs, samples_by_graph);
        std::cout << "Benchmark checksum: " << std::setprecision(17) << checksum << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
