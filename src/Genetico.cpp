#include "Algoritmos.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

// =============================================================================
// Tipos internos
// =============================================================================
using Crom    = std::vector<int>;   // permutación de índices de clientes
using Poblacion = std::vector<Crom>;

// =============================================================================
// DECODIFICADOR  →  Convierte un cromosoma en rutas CVRP reales
// =============================================================================
// El cromosoma es una lista ordenada de IDs de clientes (sin el depósito).
// El decodificador los va asignando a vehículos en orden, respetando la
// capacidad. Cuando un cliente no cabe, se cierra la ruta actual, se regresa
// al depósito y se abre una nueva con el siguiente vehículo disponible.
// Si se agotan los vehículos y quedan clientes, se aplica una penalización
// elevada para que ese cromosoma nunca sea el ganador.
//
// Retorna la distancia total (+ penalización); las rutas se escriben en
// 'rutasSalida' como vector<vector<int>> de índices en el vector 'nodos'.
// =============================================================================
static double decodificar(const Crom&                   crom,
                          const MatrizDist&              dist,
                          const std::vector<Nodo>&       nodos,
                          const std::vector<Vehiculo>&   vehiculos,
                          int                            depIdx,
                          std::vector<std::vector<int>>& rutasSalida)
{
    rutasSalida.clear();
    double distTotal = 0.0;
    size_t ci        = 0;               // cursor sobre el cromosoma

    for (size_t vi = 0; vi < vehiculos.size() && ci < crom.size(); ++vi) {
        std::vector<int> ruta;
        ruta.push_back(depIdx);

        float capacidadRestante = vehiculos[vi].capacidad;
        int   actual            = depIdx;

        // Agregar clientes mientras quepan en este vehículo
        while (ci < crom.size()) {
            int   nodoIdx = crom[ci];
            float cargaNodo = nodos[nodoIdx].cargaEfectiva();

            if (cargaNodo > capacidadRestante) break;   // no cabe → cambiar vehículo

            distTotal         += dist[actual][nodoIdx];
            capacidadRestante -= cargaNodo;
            actual             = nodoIdx;
            ruta.push_back(nodoIdx);
            ++ci;
        }

        // Regresar al depósito y cerrar la ruta
        distTotal += dist[actual][depIdx];
        ruta.push_back(depIdx);
        rutasSalida.push_back(ruta);
    }

    // Penalización si quedan clientes sin asignar (flota insuficiente para
    // esta permutación). El factor 1e9 asegura que estos individuos nunca
    // ganen la selección.
    if (ci < crom.size()) {
        distTotal += 1e9 * static_cast<double>(crom.size() - ci);
    }

    return distTotal;
}

// =============================================================================
// SELECCIÓN POR TORNEO
// =============================================================================
// Se escogen 'tamTorneo' individuos al azar de la población y se devuelve
// el de menor distancia (mayor fitness). Favorece la diversidad al no
// ordenar toda la población en cada generación.
// =============================================================================
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

// =============================================================================
// CRUZAMIENTO  —  Order Crossover OX1
// =============================================================================
// 1. Se elige un segmento aleatorio [a, b] del padre P1 y se copia tal cual
//    al hijo en las mismas posiciones.
// 2. Se recorre P2 a partir de la posición (b+1) de forma circular.
//    Cada gen de P2 que NO esté ya en el hijo se va insertando en las
//    posiciones libres del hijo, comenzando en (b+1) y también de forma
//    circular.
// Garantía: el hijo es siempre una permutación válida sin repetidos.
// =============================================================================
static Crom ox1(const Crom& p1, const Crom& p2, std::mt19937& rng)
{
    int n = static_cast<int>(p1.size());
    std::uniform_int_distribution<int> rPos(0, n - 1);

    int a = rPos(rng);
    int b = rPos(rng);
    if (a > b) std::swap(a, b);

    // Hijo inicializado con -1 (posición vacía)
    Crom hijo(n, -1);
    std::vector<bool> enHijo(/* max ID posible */ *std::max_element(p1.begin(), p1.end()) + 1, false);

    // Paso 1: copiar segmento de P1
    for (int k = a; k <= b; ++k) {
        hijo[k]        = p1[k];
        enHijo[p1[k]]  = true;
    }

    // Paso 2: rellenar con genes de P2 en orden circular desde b+1
    int pos = (b + 1) % n;
    int src = (b + 1) % n;

    for (int count = 0; count < n - (b - a + 1); ++count) {
        // Avanzar en P2 hasta encontrar un gen no presente en el hijo
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

// =============================================================================
// MUTACIÓN  —  Swap Mutation
// =============================================================================
// Selecciona dos posiciones al azar del cromosoma e intercambia sus genes.
// Mantiene la permutación válida con coste O(1).
// =============================================================================
static void swapMutation(Crom& crom, std::mt19937& rng)
{
    if (crom.size() < 2) return;
    std::uniform_int_distribution<int> rPos(0, static_cast<int>(crom.size()) - 1);
    int a = rPos(rng), b = rPos(rng);
    while (a == b) b = rPos(rng);     // garantizar posiciones distintas
    std::swap(crom[a], crom[b]);
}

// =============================================================================
// ALGORITMO GENÉTICO  —  CVRP
// =============================================================================
ResultadoAlgoritmo ejecutarGenetico(
    const MatrizDist&            dist,
    const std::vector<Nodo>&     nodos,
    const std::vector<Vehiculo>& vehiculos)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    // ── Resultado acumulador ──────────────────────────────────────────────────
    ResultadoAlgoritmo mejor;
    mejor.distanciaTotal = std::numeric_limits<double>::max();

    // ── Identificar depósito y lista de clientes ──────────────────────────────
    int depIdx = 0;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (nodos[i].esDeposito) { depIdx = i; break; }

    std::vector<int> clientes;
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i)
        if (!nodos[i].esDeposito) clientes.push_back(i);

    // Casos triviales: sin clientes o sin vehículos
    if (clientes.empty() || vehiculos.empty()) {
        mejor.distanciaTotal = 0.0;
        auto t1 = std::chrono::high_resolution_clock::now();
        mejor.tiempoMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return mejor;
    }

    // ── Hiperparámetros ───────────────────────────────────────────────────────
    constexpr int    TAM_POB    = 100;   // tamaño de la población
    constexpr int    GENERACION = 500;   // criterio de parada
    constexpr double PC         = 0.8;   // probabilidad de cruzamiento OX1
    constexpr double PM         = 0.1;   // probabilidad de mutación swap
    constexpr int    TAM_TORNEO = 5;     // participantes por torneo
    constexpr int    ELITISMO   = 2;     // mejores individuos que pasan directo

    // ── Generador de números aleatorios (semilla fija → reproducible) ─────────
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> rReal(0.0, 1.0);

    // ── Población inicial: permutaciones aleatorias de 'clientes' ────────────
    Poblacion pobla(TAM_POB, clientes);
    for (auto& crom : pobla) std::shuffle(crom.begin(), crom.end(), rng);

    // Vector de fitnesses (distancias) para la generación actual
    std::vector<double> fitnesses(TAM_POB);

    // ── Bucle evolutivo ───────────────────────────────────────────────────────
    for (int gen = 0; gen < GENERACION; ++gen) {

        // 1. EVALUACIÓN: calcular fitness de cada individuo
        for (int i = 0; i < TAM_POB; ++i) {
            std::vector<std::vector<int>> rutasTmp;
            fitnesses[i] = decodificar(pobla[i], dist, nodos, vehiculos, depIdx, rutasTmp);
        }

        // 2. ELITISMO: identificar los mejores y guardar el mejor global
        // Ordenar índices por fitness ascendente (menor distancia = mejor)
        std::vector<int> orden(TAM_POB);
        std::iota(orden.begin(), orden.end(), 0);
        std::sort(orden.begin(), orden.end(),
                  [&](int a, int b){ return fitnesses[a] < fitnesses[b]; });

        // Actualizar mejor solución global si el campeón de esta generación mejora
        if (fitnesses[orden[0]] < mejor.distanciaTotal) {
            std::vector<std::vector<int>> rutasMejor;
            mejor.distanciaTotal = decodificar(
                pobla[orden[0]], dist, nodos, vehiculos, depIdx, rutasMejor);
            mejor.rutas = rutasMejor;
        }

        // 3. NUEVA GENERACIÓN
        Poblacion nueva;
        nueva.reserve(TAM_POB);

        // Elitismo: los 'ELITISMO' mejores pasan sin modificación
        for (int e = 0; e < ELITISMO; ++e)
            nueva.push_back(pobla[orden[e]]);

        // Rellenar el resto con cruzamiento + mutación
        while (static_cast<int>(nueva.size()) < TAM_POB) {

            // SELECCIÓN: dos padres por torneo (independientes)
            const Crom& p1 = torneo(pobla, fitnesses, TAM_TORNEO, rng);
            const Crom& p2 = torneo(pobla, fitnesses, TAM_TORNEO, rng);

            // CRUZAMIENTO OX1 con probabilidad PC
            Crom hijo = (rReal(rng) < PC) ? ox1(p1, p2, rng) : p1;

            // MUTACIÓN swap con probabilidad PM
            if (rReal(rng) < PM) swapMutation(hijo, rng);

            nueva.push_back(std::move(hijo));
        }

        pobla = std::move(nueva);
    }

    // ── Si no se encontró ninguna solución válida, devolvemos vacío ──────────
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
