#include "Algoritmos.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

using Crom    = std::vector<int>;
using Poblacion = std::vector<Crom>;


static double decodificar(const Crom&                   crom,
                          const MatrizDist&              dist,
                          const std::vector<Nodo>&       nodos,
                          const std::vector<Vehiculo>&   vehiculos,
                          int                            depIdx,
                          std::vector<std::vector<int>>& rutasSalida)
{
    rutasSalida.clear();
    double distTotal = 0.0;
    size_t ci        = 0;

    for (size_t vi = 0; vi < vehiculos.size() && ci < crom.size(); ++vi) {
        std::vector<int> ruta;
        ruta.push_back(depIdx);

        float capacidadRestante = vehiculos[vi].capacidad;
        int   actual            = depIdx;

        while (ci < crom.size()) {
            int   nodoIdx = crom[ci];
            float cargaNodo = nodos[nodoIdx].cargaEfectiva();

            if (cargaNodo > capacidadRestante) break; 

            distTotal         += dist[actual][nodoIdx];
            capacidadRestante -= cargaNodo;
            actual             = nodoIdx;
            ruta.push_back(nodoIdx);
            ++ci;
        }

        distTotal += dist[actual][depIdx];
        ruta.push_back(depIdx);
        rutasSalida.push_back(ruta);
    }

    if (ci < crom.size()) {
        distTotal += 1e9 * static_cast<double>(crom.size() - ci);
    }

    return distTotal;
}


static const Crom& torneo(const Poblacion&              pobla,
                          const std::vector<double>&    fitnesses,
                          int                           tamTorneo,
                          std::mt19937&                 rng)
{
    std::uniform_int_distribution<int> rIdx(0, static_cast<int>(pobla.size()) - 1);
    int mejor = rIdx(rng);
    for (int k = 1; k < tamTorneo; ++k) {
        int candidato = rIdx(rng);
        if (fitnesses[candidato] < fitnesses[mejor])
            mejor = candidato;
    }
    return pobla[mejor];
}


static Crom ox1(const Crom& p1, const Crom& p2, std::mt19937& rng)
{
    int n = static_cast<int>(p1.size());
    std::uniform_int_distribution<int> rPos(0, n - 1);

    int a = rPos(rng);
    int b = rPos(rng);
    if (a > b) std::swap(a, b);
    Crom hijo(n, -1);
    std::vector<bool> enHijo(/* max ID posible */ *std::max_element(p1.begin(), p1.end()) + 1, false);

    for (int k = a; k <= b; ++k) {
        hijo[k]        = p1[k];
        enHijo[p1[k]]  = true;
    }
    int pos = (b + 1) % n;
    int src = (b + 1) % n;

    for (int count = 0; count < n - (b - a + 1); ++count) {
        while (enHijo[p2[src]]) {
            src = (src + 1) % n;
        }
        hijo[pos]       = p2[src];
        enHijo[p2[src]] = true;
        src = (src + 1) % n;
        pos = (pos + 1) % n;
    }

    return hijo;
}

static void swapMutation(Crom& crom, std::mt19937& rng)
{
    if (crom.size() < 2) return;
    std::uniform_int_distribution<int> rPos(0, static_cast<int>(crom.size()) - 1);
    int a = rPos(rng), b = rPos(rng);
    while (a == b) b = rPos(rng);
    std::swap(crom[a], crom[b]);
}

ResultadoAlgoritmo ejecutarGenetico(
    const MatrizDist&            dist,
    const std::vector<Nodo>&     nodos,
    const std::vector<Vehiculo>& vehiculos)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    ResultadoAlgoritmo mejor;
    mejor.distanciaTotal = std::numeric_limits<double>::max();

    int depIdx = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (nodos[i].esDeposito) { depIdx = i; break; }

    std::vector<int> clientes;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (!nodos[i].esDeposito) clientes.push_back(i);

    if (clientes.empty() || vehiculos.empty()) {
        mejor.distanciaTotal = 0.0;
        auto t1 = std::chrono::high_resolution_clock::now();
        mejor.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return mejor;
    }

    constexpr int    TAM_POB    = 100;
    constexpr int    GENERACION = 500;
    constexpr double PC         = 0.8;
    constexpr double PM         = 0.1;
    constexpr int    TAM_TORNEO = 5;
    constexpr int    ELITISMO   = 2;            

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> rReal(0.0, 1.0);

    Poblacion pobla(TAM_POB, clientes);
    for (auto& crom : pobla) std::shuffle(crom.begin(), crom.end(), rng);

    std::vector<double> fitnesses(TAM_POB);

    for (int gen = 0; gen < GENERACION; ++gen) {

        for (int i = 0; i < TAM_POB; ++i) {
            std::vector<std::vector<int>> rutasTmp;
            fitnesses[i] = decodificar(pobla[i], dist, nodos, vehiculos, depIdx, rutasTmp);
        }
        std::vector<int> orden(TAM_POB);
        std::iota(orden.begin(), orden.end(), 0);
        std::sort(orden.begin(), orden.end(),
                  [&](int a, int b){ return fitnesses[a] < fitnesses[b]; });

        if (fitnesses[orden[0]] < mejor.distanciaTotal) {
            std::vector<std::vector<int>> rutasMejor;
            mejor.distanciaTotal = decodificar(
                pobla[orden[0]], dist, nodos, vehiculos, depIdx, rutasMejor);
            mejor.rutas = rutasMejor;
        }

        Poblacion nueva;
        nueva.reserve(TAM_POB);

        for (int e = 0; e < ELITISMO; ++e)
            nueva.push_back(pobla[orden[e]]);

        while (static_cast<int>(nueva.size()) < TAM_POB) {
            const Crom& p1 = torneo(pobla, fitnesses, TAM_TORNEO, rng);
            const Crom& p2 = torneo(pobla, fitnesses, TAM_TORNEO, rng);

            Crom hijo = (rReal(rng) < PC) ? ox1(p1, p2, rng) : p1;

            if (rReal(rng) < PM) swapMutation(hijo, rng);

            nueva.push_back(std::move(hijo));
        }

        pobla = std::move(nueva);
    }

    if (mejor.rutas.empty()) {
        std::vector<std::vector<int>> rutasTmp;
        mejor.distanciaTotal = decodificar(
            pobla[0], dist, nodos, vehiculos, depIdx, rutasTmp);
        mejor.rutas = rutasTmp;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    mejor.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return mejor;
}
