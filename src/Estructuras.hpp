#pragma once
#include <string>
#include <vector>

struct Producto {
  std::string nombre;
  float peso; 
};

inline float demandaTotal(const std::vector<Producto> &prods) {
  float t = 0.f;
  for (const auto &p : prods)
    t += p.peso;
  return t;
}

struct NodoGrafo {
  int id;
  float x, y;
  std::string nombre;
  bool esAvenida;
};

struct Arista {
  int desde, hasta;
  float metros;
  bool esAvenida;
};

struct Grafo {
  std::vector<NodoGrafo> nodos;
  std::vector<Arista> aristas;
  std::vector<std::vector<std::pair<int, float>>> adj;
};

using MatrizDist = std::vector<std::vector<float>>;
using MatrizCaminos = std::vector<std::vector<std::vector<int>>>;

struct InfoRutas {
  MatrizDist distancias;
  MatrizCaminos caminos;
};

struct Nodo {
  int id;
  std::string nombre;
  float pos_x, pos_y;
  std::vector<Producto> productos;
  float demanda = 0.f;
  bool esDeposito;
  int grafoNodeId = -1;
  int vehiculoAsignado = -1;

  float cargaEfectiva() const {
    if (!productos.empty()) {
      float t = 0.f;
      for (const auto &p : productos)
        t += p.peso;
      return t;
    }
    return demanda;
  }
};

struct Vehiculo {
  int id;
  std::string placa;
  float capacidad;
  float carga_actual;
};

struct ResultadoAlgoritmo {
  std::vector<std::vector<int>> rutas;
  double distanciaTotal;
  double tiempoMs;
};
