#pragma once

#include "openflights_loader.hpp"
#include <iostream>

namespace course_test {

inline apsp::Graph small_graph() {
    return apsp::OpenFlightsLoader::load(
        DATASET_DIR "/datos_aeropuertos_prueba.csv",
        DATASET_DIR "/datos_rutas_prueba.csv",
        DATASET_DIR "/datos_seleccion_prueba.txt", 5);
}

inline void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Test>
int run(Test test) {
    try {
        test();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

}  // namespace course_test
