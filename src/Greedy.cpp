#include "Algoritmos.hpp"
#include <chrono>
#include <limits>


ResultadoAlgoritmo ejecutarGreedy(
    const MatrizDist&             dist,
    const std::vector<Nodo>&      nodos,
    const std::vector<Vehiculo>&  vehiculos)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    ResultadoAlgoritmo res;
    res.distanciaTotal = 0.0;

    if (nodos.empty() || vehiculos.empty()) {
        auto t1 = std::chrono::high_resolution_clock::now();
        res.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return res;
    }

    int depIdx = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i) {
        if (nodos[i].esDeposito) { depIdx = i; break; }
    }

    std::vector<bool> visitado(nodos.size(), false);
    visitado[depIdx] = true;

    int clientesPendientes = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (!nodos[i].esDeposito) ++clientesPendientes;

    for (size_t vi = 0; vi < vehiculos.size() && clientesPendientes > 0; ++vi) {
        const Vehiculo& v = vehiculos[vi];

        std::vector<int> rutaActual;
        rutaActual.push_back(depIdx);

        float capacidadRestante = v.capacidad;
        int   posActual         = depIdx;
        bool  seMueve           = true;

        while (seMueve) {
            seMueve = false;
            int   mejorIdx  = -1;
            float mejorDist = std::numeric_limits<float>::max();

            for (int i = 0; i < static_cast<int>(nodos.size()); ++i) {
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

            if (mejorIdx >= 0) {
                visitado[mejorIdx]  = true;
                capacidadRestante  -= nodos[mejorIdx].cargaEfectiva();
                res.distanciaTotal += mejorDist;
                rutaActual.push_back(mejorIdx);
                posActual           = mejorIdx;
                --clientesPendientes;
                seMueve             = true;
            }
        }
        res.distanciaTotal += dist[posActual][depIdx];
        rutaActual.push_back(depIdx);
        res.rutas.push_back(rutaActual);
    }

    if (clientesPendientes > 0) {
        res.distanciaTotal += 1e9 * clientesPendientes;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    res.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return res;
}
