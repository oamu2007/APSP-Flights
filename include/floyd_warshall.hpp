#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace apsp {

// Una conexion dirigida: aeropuerto de origen, destino y distancia en km.
struct Edge { int from, to; double km; };

class FloydWarshall {
    std::vector<std::vector<double>> distances_;
    std::vector<std::vector<int>> predecessors_;

    void check_vertex(int vertex) const {
        if (vertex < 0 || vertex >= static_cast<int>(distances_.size()))
            throw std::out_of_range("Vertice fuera del grafo");
    }

public:
    FloydWarshall(int n, const std::vector<Edge>& edges) {
        if (n < 1 || n > 1000)
            throw std::invalid_argument("La demo permite de 1 a 1000 aeropuertos");
        const double infinity = std::numeric_limits<double>::infinity();
        distances_.assign(n, std::vector<double>(n, infinity));
        predecessors_.assign(n, std::vector<int>(n, -1));

        // Paso 1: distancia cero al propio nodo y pesos de vuelos directos.
        for (int i = 0; i < n; ++i) distances_[i][i] = 0;
        for (const Edge& edge : edges) {
            check_vertex(edge.from);
            check_vertex(edge.to);
            if (!std::isfinite(edge.km) || edge.km < 0)
                throw std::invalid_argument("Distancia invalida");
            if (edge.km < distances_[edge.from][edge.to]) {
                distances_[edge.from][edge.to] = edge.km;
                predecessors_[edge.from][edge.to] = edge.from;
            }
        }

        // Paso 2: probar cada aeropuerto k como posible intermediario.
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    double through_k = distances_[i][k] + distances_[k][j];
                    if (through_k < distances_[i][j]) {
                        distances_[i][j] = through_k;
                        predecessors_[i][j] = predecessors_[k][j];
                    }
                }
            }
        }
    }

    double distance(int source, int target) const {
        check_vertex(source);
        check_vertex(target);
        return distances_[source][target];
    }

    // Paso 3: seguir los predecesores desde destino hasta origen.
    std::vector<int> path(int source, int target) const {
        if (!std::isfinite(distance(source, target))) return {};
        std::vector<int> result{target};
        while (target != source) {
            target = predecessors_[source][target];
            if (target < 0 || result.size() >= distances_.size())
                throw std::logic_error("Cadena de predecesores invalida");
            result.push_back(target);
        }
        std::reverse(result.begin(), result.end());
        return result;
    }
};
}
