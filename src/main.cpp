#include "airport_removal.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

// JSON is the interchange format for the command line and Python client.
static std::string quoted(const std::string& text) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}

static std::string graph_json(const apsp::Graph& graph) {
    std::ostringstream out;
    out << std::setprecision(12) << "\"nodes\":[";
    for (std::size_t i = 0; i < graph.airports.size(); ++i) {
        const auto& a = graph.airports[i];
        out << (i ? "," : "") << "{\"code\":" << quoted(a.code) << ",\"name\":" << quoted(a.name)
            << ",\"city\":" << quoted(a.city) << ",\"country\":" << quoted(a.country)
            << ",\"lat\":" << a.latitude << ",\"lon\":" << a.longitude << '}';
    }
    out << "],\"edges\":[";
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& e = graph.edges[i];
        out << (i ? "," : "") << '[' << quoted(graph.airports[e.from].code) << ','
            << quoted(graph.airports[e.to].code) << ',' << e.km << ']';
    }
    return out.str() + ']';
}

static std::string route_json(const apsp::Graph& graph, const apsp::FloydWarshall& solver,
                              const std::string& source, const std::string& target) {
    int from = graph.find(source), to = graph.find(target);
    auto path = solver.path(from, to);
    std::ostringstream out;
    out << std::setprecision(12) << "\"source\":" << quoted(source) << ",\"target\":" << quoted(target)
        << ",\"distance_km\":";
    if (path.empty()) out << "null";
    else out << solver.distance(from, to);
    out << ",\"path\":[";
    for (std::size_t i = 0; i < path.size(); ++i)
        out << (i ? "," : "") << quoted(graph.airports[path[i]].code);
    return out.str() + ']';
}

static std::string impact_json(const apsp::RemovalImpact& impact) {
    std::ostringstream out;
    out << std::setprecision(12);
    const auto ratio = [&out](double numerator, int denominator) {
        if (denominator) out << numerator / denominator;
        else out << "null";
    };
    out << "{\"baseline_reachable_pairs\":" << impact.baseline_reachable
        << ",\"lost_pairs\":" << impact.lost << ",\"lost_share\":";
    ratio(impact.lost, impact.baseline_reachable);
    out << ",\"still_reachable_pairs\":" << impact.still_reachable
        << ",\"detoured_pairs\":" << impact.detoured << ",\"detoured_share\":";
    ratio(impact.detoured, impact.still_reachable);
    out << ",\"mean_detour_km\":";
    ratio(impact.total_detour_km, impact.detoured);
    out << ",\"max_detour_km\":";
    if (impact.detoured) out << impact.max_detour_km;
    else out << "null";
    return out.str() + '}';
}

class FlightNetwork {
    apsp::Graph baseline_;
    apsp::FloydWarshall solver_;
    std::optional<apsp::RemovalScenario> scenario_;

public:
    explicit FlightNetwork(apsp::Graph graph)
        : baseline_(std::move(graph)),
          solver_(static_cast<int>(baseline_.airports.size()), baseline_.edges) {}

    void remove(const std::string& code) {
        if (scenario_ && baseline_.airports[scenario_->impact.airport].code == code) return;
        scenario_ = apsp::make_removal_scenario(baseline_, solver_, baseline_.find(code));
    }

    void restore() { scenario_.reset(); }

    std::string graph_fields() const {
        if (!scenario_) return graph_json(baseline_);
        return graph_json(scenario_->graph) +
            ",\"removed_airport\":" + quoted(baseline_.airports[scenario_->impact.airport].code) +
            ",\"impact\":" + impact_json(scenario_->impact);
    }

    std::string route_fields(const std::string& source, const std::string& target) const {
        if (!scenario_) return route_json(baseline_, solver_, source, target);
        const auto& removed = baseline_.airports[scenario_->impact.airport].code;
        if (source == removed || target == removed)
            throw std::invalid_argument("Airport " + removed + " is removed in this scenario");
        if (!scenario_->solver) throw std::invalid_argument("No airports remain in this scenario");
        return route_json(scenario_->graph, *scenario_->solver, source, target);
    }
};

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> args{
            {"--airports", "dataset/datos_aeropuertos_validacion.dat"},
            {"--routes", "dataset/datos_rutas_validacion.dat"},
            {"--selection", "dataset/aeropuertos_demo.txt"},
            {"--from", "MEX"}, {"--to", "NRT"}, {"--limit", ""}, {"--preset", "historical"}};
        bool serve = false, explicit_limit = false;
        std::optional<std::string> initial_removal;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--serve") { serve = true; continue; }
            if (key == "--help") {
                std::cout << "APSP-Flights command-line tool (apsp_demo)\n"
                    << "Usage: apsp_demo [--from MEX] [--to NRT] [--preset historical|expanded] [--limit N] [--remove IATA]\n"
                    << "Historical uses the selection file (default 150); expanded adds airports by degree (default 500).\n"
                    << "--serve accepts IATA pairs, --remove IATA, and --restore on standard input.\n"
                    << "Alternative input files: --airports, --routes, --selection.\n";
                return 0;
            }
            if (key == "--remove" && i + 1 < argc) {
                initial_removal = argv[++i];
                continue;
            }
            if (!args.count(key) || i + 1 >= argc)
                throw std::invalid_argument("Invalid argument: " + key);
            args[key] = argv[++i];
            if (key == "--limit") explicit_limit = true;
        }
        const bool expanded = args["--preset"] == "expanded";
        if (!expanded && args["--preset"] != "historical")
            throw std::invalid_argument("Unknown airport preset: " + args["--preset"]);
        const int limit = explicit_limit ? apsp::OpenFlightsLoader::integer(args["--limit"])
            : (expanded ? apsp::OpenFlightsLoader::expanded_size : apsp::OpenFlightsLoader::historical_size);
        FlightNetwork network(apsp::OpenFlightsLoader::load(args["--airports"], args["--routes"],
            args["--selection"], limit, expanded));
        if (initial_removal) network.remove(*initial_removal);
        if (!serve) {
            auto route = network.route_fields(args["--from"], args["--to"]);
            std::cout << '{' << network.graph_fields() << ',' << route << "}\n";
            return 0;
        }
        std::cout << '{' << network.graph_fields() << '}' << std::endl;
        std::string source, target;
        while (std::cin >> source) {
            try {
                std::string fields;
                if (source == "--restore") {
                    network.restore();
                    fields = network.graph_fields();
                } else if (std::cin >> target) {
                    if (source == "--remove") {
                        network.remove(target);
                        fields = network.graph_fields();
                    } else {
                        fields = network.route_fields(source, target);
                    }
                } else {
                    break;
                }
                std::cout << '{' << fields << '}' << std::endl;
            } catch (const std::invalid_argument& error) {
                std::cout << "{\"error\":" << quoted(error.what()) << '}' << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
