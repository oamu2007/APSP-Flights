#include "network.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace {

namespace fs = std::filesystem;

std::ofstream open_csv(const fs::path& path) {
    std::ofstream out;
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out.open(path);
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    return out;
}

std::string csv_field(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"') quoted += '"';
        quoted += c;
    }
    return quoted + '"';
}

int record_count(const fs::path& input) {
    std::ifstream file(input);
    if (!file) throw std::runtime_error("Could not open " + input.string());
    int records = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line != "\r") ++records;
    }
    if (file.bad()) throw std::runtime_error("Could not read " + input.string());
    return records;
}

void write_profile(const fs::path& output, const apsp::Graph& graph,
                   const evaluation::NetworkProfile& profile, int airport_rows, int route_rows) {
    const auto n = graph.airports.size();
    const auto distinct_pairs = n * (n - 1);
    std::set<std::string> countries;
    for (const auto& airport : graph.airports) countries.insert(airport.country);

    auto summary = open_csv(output / "graph_summary.csv");
    summary << "metric,value\n"
            << "raw_airport_rows," << airport_rows << '\n'
            << "raw_route_rows," << route_rows << '\n'
            << "airports," << n << '\n'
            << "directed_edges," << graph.edges.size() << '\n'
            << "distinct_ordered_pairs," << distinct_pairs << '\n'
            << "directed_density," << static_cast<double>(graph.edges.size()) / distinct_pairs << '\n'
            << "directed_edges_without_reverse," << profile.one_way_edges << '\n'
            << "reachable_pairs," << profile.reachable_pairs << '\n'
            << "unreachable_pairs," << distinct_pairs - profile.reachable_pairs << '\n'
            << "reachable_pairs_without_reverse," << profile.one_way_pairs << '\n'
            << "strongly_connected_components," << profile.component_sizes.size() << '\n'
            << "country_or_territory_labels," << countries.size() << '\n';
    summary.close();

    auto degrees = open_csv(output / "airport_degrees.csv");
    degrees << "airport,country_or_territory,latitude,longitude,in_degree,out_degree,component\n";
    for (std::size_t i = 0; i < n; ++i) {
        const auto& airport = graph.airports[i];
        const auto& degree = profile.degrees[i];
        degrees << airport.code << ',' << csv_field(airport.country) << ','
                << airport.latitude << ',' << airport.longitude << ','
                << degree.in << ',' << degree.out << ',' << profile.component_by_airport[i] + 1 << '\n';
    }
    degrees.close();

    auto components = open_csv(output / "components.csv");
    components << "component,airports\n";
    for (std::size_t i = 0; i < profile.component_sizes.size(); ++i) {
        components << i + 1 << ',' << profile.component_sizes[i] << '\n';
    }
    components.close();
}

void write_fraction(std::ostream& out, double numerator, int denominator) {
    if (denominator > 0) out << numerator / denominator;
}

void write_removals(const fs::path& output, const apsp::Graph& graph,
                    const apsp::FloydWarshall& solver) {
    std::vector<evaluation::RemovalImpact> impacts;
    for (int i = 0; i < static_cast<int>(graph.airports.size()); ++i) {
        impacts.push_back(evaluation::remove_airport(graph, solver, i));
    }
    std::sort(impacts.begin(), impacts.end(), [&graph](const auto& a, const auto& b) {
        if (a.lost != b.lost) return a.lost > b.lost;
        return graph.airports[a.airport].code < graph.airports[b.airport].code;
    });

    auto out = open_csv(output / "airport_removal.csv");
    out << "airport,baseline_reachable_pairs,lost_pairs,lost_share,still_reachable_pairs,"
           "detoured_pairs,detoured_share,mean_detour_km,max_detour_km\n";
    for (const auto& impact : impacts) {
        out << graph.airports[impact.airport].code << ',' << impact.baseline_reachable << ','
            << impact.lost << ',';
        write_fraction(out, impact.lost, impact.baseline_reachable);
        out << ',' << impact.still_reachable << ',' << impact.detoured << ',';
        write_fraction(out, impact.detoured, impact.still_reachable);
        out << ',';
        write_fraction(out, impact.total_detour_km, impact.detoured);
        out << ',';
        if (impact.detoured > 0) out << impact.max_detour_km;
        out << '\n';
    }
    out.close();
}

void write_longest(const fs::path& output, const apsp::Graph& graph,
                   const apsp::FloydWarshall& solver) {
    auto paths = open_csv(output / "longest_shortest_paths.csv");
    paths << "rank,source,target,distance_km,hops,path\n";
    int rank = 0;
    for (const auto& pair : evaluation::longest_shortest_paths(graph, solver, 10)) {
        const auto path = solver.path(pair.from, pair.to);
        paths << ++rank << ',' << graph.airports[pair.from].code << ',' << graph.airports[pair.to].code
              << ',' << pair.km << ',' << path.size() - 1 << ',';
        const char* separator = "";
        for (int vertex : path) {
            paths << separator << graph.airports[vertex].code;
            separator = " ";
        }
        paths << '\n';
    }
    paths.close();

    auto direct = open_csv(output / "longest_direct_edge.csv");
    direct << "source,target,distance_km\n";
    if (const auto edge = evaluation::longest_direct_edge(graph)) {
        direct << graph.airports[edge->from].code << ',' << graph.airports[edge->to].code
               << ',' << edge->km << '\n';
    }
    direct.close();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: apsp_analysis <output-directory> (run from repository root)\n";
        return 2;
    }
    try {
        const fs::path output(argv[1]);
        const std::string airports = "dataset/datos_aeropuertos_validacion.dat";
        const std::string routes = "dataset/datos_rutas_validacion.dat";
        const auto graph = apsp::OpenFlightsLoader::load(airports, routes, "dataset/aeropuertos_demo.txt");
        const apsp::FloydWarshall solver(static_cast<int>(graph.airports.size()), graph.edges);
        fs::create_directories(output);
        write_profile(output, graph, evaluation::profile(graph, solver), record_count(airports), record_count(routes));
        write_removals(output, graph, solver);
        write_longest(output, graph, solver);
    } catch (const std::exception& error) {
        std::cerr << "Analysis failed: " << error.what() << '\n';
        return 1;
    }
}
