#include "Algoritmos.hpp"
#include <chrono>
#include <limits>

// =============================================================================
// Algoritmo Greedy CVRP — Nearest Neighbor Heuristic
// =============================================================================
// Estrategia:
//   1. El depósito (nodos[depIdx]) es el punto de origen y retorno de cada ruta.
//   2. Se itera sobre la flota de vehículos disponibles en orden.
//   3. Cada vehículo parte del depósito con su capacidad máxima.
//   4. En cada paso se elige el cliente no visitado MÁS CERCANO (usando la
//      MatrizDist precomputada con Dijkstra) cuya cargaEfectiva() no exceda
//      la capacidad restante del vehículo actual.
//   5. Cuando ningún cliente cabe, el vehículo regresa al depósito y se
//      asigna el siguiente vehículo. El proceso continúa hasta que todos
//      los clientes han sido visitados o se agota la flota.
//   6. Toda ruta termina obligatoriamente en el depósito.
//   7. Si quedan clientes sin asignar (flota insuficiente), se penaliza
//      la distancia total con 1e9 por cliente sin atender.
// =============================================================================

ResultadoAlgoritmo ejecutarGreedy(
    const MatrizDist&             dist,
    const std::vector<Nodo>&      nodos,
    const std::vector<Vehiculo>&  vehiculos)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    ResultadoAlgoritmo res;
    res.distanciaTotal = 0.0;

    // ── Guardia: casos triviales ──────────────────────────────────────────────
    if (nodos.empty() || vehiculos.empty()) {
        auto t1 = std::chrono::high_resolution_clock::now();
        res.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return res;
    }

    // ── Encontrar el depósito (siempre nodos[0] según la lógica del bridge) ──
    int depIdx = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i) {
        if (nodos[i].esDeposito) { depIdx = i; break; }
    }

    // ── Registro de clientes visitados ────────────────────────────────────────
    // visitado[depIdx] = true desde el inicio: el depósito nunca se "visita"
    // como cliente.
    std::vector<bool> visitado(nodos.size(), false);
    visitado[depIdx] = true;

    // Cuenta cuántos clientes quedan pendientes para la penalización final
    int clientesPendientes = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (!nodos[i].esDeposito) ++clientesPendientes;

    // ── Bucle principal: un vehículo por iteración ────────────────────────────
    for (size_t vi = 0; vi < vehiculos.size() && clientesPendientes > 0; ++vi) {
        const Vehiculo& v = vehiculos[vi];

        std::vector<int> rutaActual;
        rutaActual.push_back(depIdx);          // toda ruta comienza en el depósito

        float capacidadRestante = v.capacidad;
        int   posActual         = depIdx;
        bool  seMueve           = true;

        // ── Nearest Neighbor: avanzar mientras haya clientes alcanzables ──────
        while (seMueve) {
            seMueve = false;
            int   mejorIdx  = -1;
            float mejorDist = std::numeric_limits<float>::max();

            for (int i = 0; i < static_cast<int>(nodos.size()); ++i) {
                // Omitir: ya visitado, no cabe en el vehículo actual,
                // o asignado a otro vehículo específico
                if (visitado[i]) continue;
                if (nodos[i].cargaEfectiva() > capacidadRestante) continue;
                if (nodos[i].vehiculoAsignado != -1 &&
                    nodos[i].vehiculoAsignado != static_cast<int>(vi)) continue;

                float d = dist[posActual][i];
                if (d < mejorDist) {
                    mejorDist = d;
                    mejorIdx  = i;
                }
            }

            // Si encontramos un cliente alcanzable, nos movemos hacia él
            if (mejorIdx >= 0) {
                visitado[mejorIdx]  = true;
                capacidadRestante  -= nodos[mejorIdx].cargaEfectiva();
                res.distanciaTotal += mejorDist;
                rutaActual.push_back(mejorIdx);
                posActual           = mejorIdx;
                --clientesPendientes;
                seMueve             = true;
            }
            // Si no encontramos ninguno (vehículo lleno o sin candidatos),
            // el while termina y el vehículo regresa al depósito (abajo).
        }

        // ── Regresar al depósito y cerrar la ruta ─────────────────────────────
        res.distanciaTotal += dist[posActual][depIdx];
        rutaActual.push_back(depIdx);
        res.rutas.push_back(rutaActual);
    }

    // ── Penalización si quedan clientes sin atender (flota insuficiente) ──────
    if (clientesPendientes > 0) {
        res.distanciaTotal += 1e9 * clientesPendientes;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    res.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return res;
}
