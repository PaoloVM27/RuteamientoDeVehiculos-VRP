#pragma once
#include <vector>
#include "Estructuras.hpp"

ResultadoAlgoritmo ejecutarGreedy(
    const MatrizDist& dist,
    const std::vector<Nodo>& nodos,
    const std::vector<Vehiculo>& vehiculos);

ResultadoAlgoritmo ejecutarFuerzaBruta(
    const MatrizDist& dist,
    const std::vector<Nodo>& nodos,
    const std::vector<Vehiculo>& vehiculos);

ResultadoAlgoritmo ejecutarGenetico(
    const MatrizDist& dist,
    const std::vector<Nodo>& nodos,
    const std::vector<Vehiculo>& vehiculos);
