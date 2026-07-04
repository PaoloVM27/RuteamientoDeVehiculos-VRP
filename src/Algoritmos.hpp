#pragma once
#include <vector>
#include "Estructuras.hpp"

// Los 3 algoritmos ahora reciben la MatrizDist precomputada con Dijkstra.
// Su lógica interna (greedy nearest-neighbor, permutaciones, genético) NO cambia.
// El único cambio es dist[i][j] en lugar de distM(nodos[i], nodos[j]).

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
