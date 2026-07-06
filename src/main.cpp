#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QHeaderView>
#include <QTableWidget>
#include <QDoubleValidator>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <limits>

// Headers propios del proyecto — fuente única de verdad para las estructuras
#include "Algoritmos.hpp"
#include "Estructuras.hpp"

// ==========================================
// Clase VrpBridge (Comunicación Qt <-> JS)
// ==========================================

class VrpBridge : public QObject {
  Q_OBJECT
public:
  explicit VrpBridge(QObject *parent = nullptr) : QObject(parent) {}

  std::vector<Nodo> listaNodos;         // Nodo de Estructuras.hpp
  std::vector<Vehiculo> listaVehiculos; // Vehiculo de Estructuras.hpp

  // Calcula la MatrizDist (float) con distancia euclidiana entre pos_x/pos_y.
  // Los algoritmos reciben esta matriz en lugar de calcularla internamente.
  MatrizDist calcularMatrizDistancias() const {
    int n = static_cast<int>(listaNodos.size());
    MatrizDist matriz(n, std::vector<float>(n, 0.f));
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (i != j) {
          float dx = listaNodos[i].pos_x - listaNodos[j].pos_x;
          float dy = listaNodos[i].pos_y - listaNodos[j].pos_y;
          matriz[i][j] = std::sqrt(dx * dx + dy * dy);
        }
      }
    }
    return matriz;
  }

public slots:
  // ── Invocado por JS al colocar/reemplazar el depósito central ────────────
  Q_INVOKABLE void recibirDeposito(double lat, double lng, QString nombre) {
    Nodo dep;
    dep.pos_x = static_cast<float>(lng);
    dep.pos_y = static_cast<float>(lat);
    dep.nombre = nombre.toStdString();
    dep.esDeposito = true;
    dep.demanda = 0.f;

    if (!listaNodos.empty() && listaNodos[0].esDeposito) {
      dep.id = 0;
      dep.grafoNodeId = 0;
      listaNodos[0] = dep;
    } else {
      dep.id = 0;
      dep.grafoNodeId = 0;
      listaNodos.insert(listaNodos.begin(), dep);
      for (int i = 1; i < static_cast<int>(listaNodos.size()); ++i) {
        listaNodos[i].id = i;
        listaNodos[i].grafoNodeId = i;
      }
    }
  }

  // ── Invocado por JS al agregar un cliente (tienda/punto de entrega) ──────
  Q_INVOKABLE void recibirCoordenadasCliente(double lat, double lng,
                                             QString nombre, double demanda) {
    Nodo n;
    n.id = static_cast<int>(listaNodos.size());
    n.pos_x = static_cast<float>(lng);
    n.pos_y = static_cast<float>(lat);
    n.nombre = nombre.toStdString();
    n.esDeposito = false;
    n.demanda = static_cast<float>(demanda);
    n.grafoNodeId = n.id;
    listaNodos.push_back(n);
  }

  // ── Invocado por JS cuando el usuario arrastra un marcador existente ──────
  // Busca el nodo por nombre y actualiza sus coordenadas.
  Q_INVOKABLE void actualizarPosicionNodo(QString nombre, double lat,
                                          double lng) {
    std::string stdNombre = nombre.toStdString();
    for (auto &n : listaNodos) {
      if (n.nombre == stdNombre) {
        n.pos_x = static_cast<float>(lng);
        n.pos_y = static_cast<float>(lat);
        return;
      }
    }
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // ==========================================
  // Configuración de la Ventana Principal
  // ==========================================
  QMainWindow mainWindow;
  mainWindow.setWindowTitle("VRP Logística");
  mainWindow.resize(1280, 720);

  QSplitter *splitter = new QSplitter(Qt::Horizontal, &mainWindow);
  mainWindow.setCentralWidget(splitter);

  // ==========================================
  // Panel Izquierdo: Controles
  // ==========================================
  QWidget *leftPanel = new QWidget(splitter);
  leftPanel->setMaximumWidth(320);
  leftPanel->setMinimumWidth(280);
  QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setSpacing(15);
  leftLayout->setContentsMargins(20, 20, 20, 20);

  // Sección Depósito
  QLabel *lblDeposito = new QLabel("<b>Agregar Depósito</b>", leftPanel);
  leftLayout->addWidget(lblDeposito);

  QHBoxLayout *depositoLayout = new QHBoxLayout();
  QLineEdit *txtNombreDeposito = new QLineEdit(leftPanel);
  txtNombreDeposito->setPlaceholderText("Nombre Depósito");
  QPushButton *btnAgregarDeposito =
      new QPushButton("Agregar Depósito", leftPanel);
  depositoLayout->addWidget(txtNombreDeposito);
  depositoLayout->addWidget(btnAgregarDeposito);
  leftLayout->addLayout(depositoLayout);

  leftLayout->addSpacing(10);

  // Sección Vehículos
  QLabel *lblVehiculo = new QLabel("<b>Agregar Vehículo</b>", leftPanel);
  leftLayout->addWidget(lblVehiculo);

  QHBoxLayout *vehiculoLayout = new QHBoxLayout();
  QLineEdit *txtPlaca = new QLineEdit(leftPanel);
  txtPlaca->setPlaceholderText("Placa");
  QLineEdit *txtCapacidad = new QLineEdit(leftPanel);
  txtCapacidad->setPlaceholderText("Capacidad");
  txtCapacidad->setValidator(new QDoubleValidator(0.0, 1e9, 2, leftPanel));
  QPushButton *btnAgregarVehiculo =
      new QPushButton("Agregar Vehículo", leftPanel);
  vehiculoLayout->addWidget(txtPlaca);
  vehiculoLayout->addWidget(txtCapacidad);
  vehiculoLayout->addWidget(btnAgregarVehiculo);
  leftLayout->addLayout(vehiculoLayout);

  // Lista visual de vehículos registrados
  QListWidget *listaVehiculosUI = new QListWidget(leftPanel);
  listaVehiculosUI->setMaximumHeight(80);
  listaVehiculosUI->setFocusPolicy(Qt::StrongFocus);
  leftLayout->addWidget(listaVehiculosUI);

  QPushButton *btnEliminarVehiculo = new QPushButton("Eliminar vehiculo seleccionado", leftPanel);
  btnEliminarVehiculo->setStyleSheet(
      "QPushButton { background-color:#c0392b; color:white; font-weight:bold;"
      " padding:5px; border-radius:4px; font-size:11px; }"
      "QPushButton:hover { background-color:#a93226; }");
  btnEliminarVehiculo->setCursor(Qt::PointingHandCursor);
  leftLayout->addWidget(btnEliminarVehiculo);

  leftLayout->addSpacing(6);

  // Sección Tiendas
  QLabel *lblTienda = new QLabel("<b>Agregar Tienda</b>", leftPanel);
  leftLayout->addWidget(lblTienda);

  QHBoxLayout *tiendaLayout = new QHBoxLayout();
  QLineEdit *txtNombreTienda = new QLineEdit(leftPanel);
  txtNombreTienda->setPlaceholderText("Nombre Tienda");
  QLineEdit *txtDemandaTienda = new QLineEdit(leftPanel);
  txtDemandaTienda->setPlaceholderText("Demanda");
  txtDemandaTienda->setValidator(new QDoubleValidator(0.0, 1e9, 2, leftPanel));
  QPushButton *btnAgregarTienda = new QPushButton("Agregar Tienda", leftPanel);
  tiendaLayout->addWidget(txtNombreTienda);
  tiendaLayout->addWidget(txtDemandaTienda);
  tiendaLayout->addWidget(btnAgregarTienda);
  leftLayout->addLayout(tiendaLayout);

  // Lista visual de tiendas registradas
  QListWidget *listaTiendasUI = new QListWidget(leftPanel);
  listaTiendasUI->setMaximumHeight(80);
  listaTiendasUI->setFocusPolicy(Qt::StrongFocus);
  leftLayout->addWidget(listaTiendasUI);

  QPushButton *btnEliminarTienda = new QPushButton("Eliminar tienda seleccionada", leftPanel);
  btnEliminarTienda->setStyleSheet(
      "QPushButton { background-color:#c0392b; color:white; font-weight:bold;"
      " padding:5px; border-radius:4px; font-size:11px; }"
      "QPushButton:hover { background-color:#a93226; }");
  btnEliminarTienda->setCursor(Qt::PointingHandCursor);
  leftLayout->addWidget(btnEliminarTienda);

  QLabel *lblDemandaTotal = new QLabel("Demanda total del viaje: 0", leftPanel);
  leftLayout->addWidget(lblDemandaTotal);

  // Botones Guardar / Cargar sesión
  QHBoxLayout *sesionLayout = new QHBoxLayout();
  QPushButton *btnGuardar = new QPushButton("Guardar sesión", leftPanel);
  QPushButton *btnCargar = new QPushButton("Cargar sesión", leftPanel);
  btnGuardar->setStyleSheet("QPushButton {"
                            "   background-color: #5c6bc0;"
                            "   color: white;"
                            "   font-weight: bold;"
                            "   padding: 8px;"
                            "   border-radius: 5px;"
                            "}"
                            "QPushButton:hover { background-color: #3f51b5; }");
  btnCargar->setStyleSheet("QPushButton {"
                           "   background-color: #00897b;"
                           "   color: white;"
                           "   font-weight: bold;"
                           "   padding: 8px;"
                           "   border-radius: 5px;"
                           "}"
                           "QPushButton:hover { background-color: #00695c; }");
  btnGuardar->setCursor(Qt::PointingHandCursor);
  btnCargar->setCursor(Qt::PointingHandCursor);
  sesionLayout->addWidget(btnGuardar);
  sesionLayout->addWidget(btnCargar);
  leftLayout->addLayout(sesionLayout);

  leftLayout->addSpacing(10);

  // Selector de Algoritmo
  QLabel *lblAlgoritmo = new QLabel("<b>Seleccionar Algoritmo</b>", leftPanel);
  leftLayout->addWidget(lblAlgoritmo);

  QComboBox *comboAlgoritmo = new QComboBox(leftPanel);
  comboAlgoritmo->addItems({"Greedy", "Fuerza Bruta", "Genético"});
  comboAlgoritmo->setMinimumHeight(30);
  leftLayout->addWidget(comboAlgoritmo);

  leftLayout->addStretch();

  // Botón Calcular Ruta
  QPushButton *btnCalcular = new QPushButton("CALCULAR RUTA", leftPanel);
  btnCalcular->setStyleSheet("QPushButton {"
                             "   background-color: #4CAF50;"
                             "   color: white;"
                             "   font-weight: bold;"
                             "   padding: 15px;"
                             "   border-radius: 5px;"
                             "   font-size: 14px;"
                             "}"
                             "QPushButton:hover {"
                             "   background-color: #45a049;"
                             "}");
  btnCalcular->setCursor(Qt::PointingHandCursor);
  leftLayout->addWidget(btnCalcular);

  // Botón Comparar Algoritmos
  QPushButton *btnComparar = new QPushButton("COMPARAR ALGORITMOS", leftPanel);
  btnComparar->setStyleSheet("QPushButton {"
                             "   background-color: #2196F3;"
                             "   color: white;"
                             "   font-weight: bold;"
                             "   padding: 10px;"
                             "   border-radius: 5px;"
                             "   font-size: 13px;"
                             "}"
                             "QPushButton:hover {"
                             "   background-color: #1e88e5;"
                             "}");
  btnComparar->setCursor(Qt::PointingHandCursor);
  leftLayout->addWidget(btnComparar);

  // Panel de resultados educativo — muestra detalle por ruta después de calcular
  QLabel *lblResultados = new QLabel("<b>Resultados del Calculo</b>", leftPanel);
  leftLayout->addWidget(lblResultados);

  QTextBrowser *resultsBrowser = new QTextBrowser(leftPanel);
  resultsBrowser->setMinimumHeight(220);
  resultsBrowser->setOpenExternalLinks(false);
  resultsBrowser->setStyleSheet(
      "QTextBrowser {"
      "   background-color: #fdfdfd;"
      "   color: #212529;"
      "   border: 1px solid #ced4da;"
      "   border-radius: 6px;"
      "   padding: 8px;"
      "   font-family: Consolas, 'Courier New', monospace;"
      "   font-size: 11px;"
      "}");
  resultsBrowser->setHtml(
      "<center style='color:#adb5bd; margin-top:50px;'>"
      "<i>Ejecuta un algoritmo<br>para ver los resultados aqu&iacute;.</i>"
      "</center>");
  leftLayout->addWidget(resultsBrowser);

  leftLayout->addStretch();

  // ==========================================
  // Panel Derecho: Mapa (QWebEngineView)
  // ==========================================
  QWebEngineView *mapView = new QWebEngineView(splitter);
  mapView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

  // Instanciar nuestro bridge para comunicar C++ y JS
  VrpBridge *bridge = new VrpBridge(&mainWindow);

  // Configurar el QWebChannel
  QWebChannel *channel = new QWebChannel(mapView->page());
  channel->registerObject("interfazBridge", bridge);
  mapView->page()->setWebChannel(channel);

  // Cargar mapa.html desde el directorio del ejecutable
  // (POST_BUILD en CMakeLists.txt copia resources/mapa.html aquí)
  QString rutaMapa = QCoreApplication::applicationDirPath() + "/mapa.html";
  // Fallback: si no existe, intenta en la subcarpeta resources/ (desarrollo
  // local)
  if (!QFile::exists(rutaMapa))
    rutaMapa = QCoreApplication::applicationDirPath() + "/resources/mapa.html";
  mapView->setUrl(QUrl::fromLocalFile(rutaMapa));

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);

  // ==========================================
  // Eventos (Signals y Slots)
  // ==========================================

  // Botón Agregar Depósito → crea marcador draggable en el centro del mapa
  QObject::connect(btnAgregarDeposito, &QPushButton::clicked, [=]() {
    QString nombre = txtNombreDeposito->text().trimmed();
    if (nombre.isEmpty())
      nombre = "Depósito Central"; // nombre por defecto
    nombre.replace("'", "\\'");
    mapView->page()->runJavaScript(
        QString("agregarMarcadorDeposito('%1');").arg(nombre));
    txtNombreDeposito->clear();
  });

  // Botón Agregar Vehículo → registra en C++ Y muestra en la lista visual
  QObject::connect(btnAgregarVehiculo, &QPushButton::clicked, [=]() {
    QString placa = txtPlaca->text().trimmed();
    float capacidad = txtCapacidad->text().toFloat();
    if (placa.isEmpty())
      placa = QString("V%1").arg(bridge->listaVehiculos.size() + 1);
    Vehiculo v;
    v.placa = placa.toStdString();
    v.capacidad = capacidad;
    bridge->listaVehiculos.push_back(v);
    // Mostrar en la lista visual con prefijo V{indice}
    int vIdx = static_cast<int>(bridge->listaVehiculos.size()) - 1;
    listaVehiculosUI->addItem(
        QString("V%1  |  %2  |  Cap: %3").arg(vIdx).arg(placa).arg(capacidad));
    txtPlaca->clear();
    txtCapacidad->clear();
  });

  // Botón Agregar Tienda → crea marcador draggable en el centro del mapa
  // Usamos un contador para generar nombres únicos por defecto
  int contTienda = 1;
  double totalDemandaAcumulada = 0.0;
  QObject::connect(btnAgregarTienda, &QPushButton::clicked, [&]() {
    QString nombre = txtNombreTienda->text().trimmed();
    double demanda = txtDemandaTienda->text().toDouble();
    if (nombre.isEmpty())
      nombre = QString("Tienda %1").arg(contTienda);
    contTienda++;
    totalDemandaAcumulada += demanda;
    lblDemandaTotal->setText(
        QString("Demanda total del viaje: %1").arg(totalDemandaAcumulada));
    // Agregar a la lista visual
    QListWidgetItem *item = new QListWidgetItem(
        QString("%1  |  Dem: %2").arg(nombre).arg(demanda));
    item->setData(Qt::UserRole, nombre);
    listaTiendasUI->addItem(item);
    QString nombreJS = nombre;
    nombreJS.replace("'", "\\'");
    mapView->page()->runJavaScript(
        QString("agregarMarcadorCliente('%1', %2);").arg(nombreJS).arg(demanda));
    txtNombreTienda->clear();
    txtDemandaTienda->clear();
  });

  // Botón Eliminar Vehículo seleccionado
  QObject::connect(btnEliminarVehiculo, &QPushButton::clicked, [=, &mainWindow]() {
    int row = listaVehiculosUI->currentRow();
    if (row < 0) {
      QMessageBox::information(&mainWindow, "Sin seleccion",
                               "Selecciona un vehiculo de la lista para eliminarlo.");
      return;
    }
    if (row < static_cast<int>(bridge->listaVehiculos.size()))
      bridge->listaVehiculos.erase(bridge->listaVehiculos.begin() + row);
    delete listaVehiculosUI->takeItem(row);
  });

  // Botón Eliminar Tienda seleccionada
  QObject::connect(btnEliminarTienda, &QPushButton::clicked, [=, &mainWindow, &totalDemandaAcumulada]() {
    QListWidgetItem *item = listaTiendasUI->currentItem();
    if (!item) {
      QMessageBox::information(&mainWindow, "Sin seleccion",
                               "Selecciona una tienda de la lista para eliminarla.");
      return;
    }
    QString nombre = item->data(Qt::UserRole).toString();
    // Buscar y eliminar de bridge->listaNodos
    auto &nodos = bridge->listaNodos;
    for (auto it = nodos.begin(); it != nodos.end(); ++it) {
      if (!it->esDeposito && QString::fromStdString(it->nombre) == nombre) {
        totalDemandaAcumulada -= static_cast<double>(it->demanda);
        nodos.erase(it);
        break;
      }
    }
    // Re-indexar nodos restantes
    for (int i = 0; i < static_cast<int>(nodos.size()); ++i) {
      nodos[i].id = i;
      nodos[i].grafoNodeId = i;
    }
    lblDemandaTotal->setText(
        QString("Demanda total del viaje: %1").arg(totalDemandaAcumulada));
    // Eliminar marcador del mapa
    QString nombreJS = nombre;
    nombreJS.replace("'", "\\'");
    mapView->page()->runJavaScript(
        QString("if (typeof eliminarMarcadorCliente === 'function') "
                "eliminarMarcadorCliente('%1');").arg(nombreJS));
    // Limpiar rutas (ya no son válidas)
    mapView->page()->runJavaScript(
        "if (typeof limpiarRutasPantalla === 'function') limpiarRutasPantalla();");
    resultsBrowser->setHtml(
        "<center style='color:#888; margin-top:40px;'>"
        "<i>Recalcula la ruta tras eliminar una tienda.</i></center>");
    delete listaTiendasUI->takeItem(listaTiendasUI->currentRow());
  });

  // ── Guardar sesión ────────────────────────────────────────────────────
  // Serializa depósito + tiendas + vehículos a un archivo .vrp (JSON)
  QObject::connect(btnGuardar, &QPushButton::clicked, [=, &mainWindow]() {
    if (bridge->listaNodos.empty() && bridge->listaVehiculos.empty()) {
      QMessageBox::warning(&mainWindow, "Sesión vacía",
                           "No hay datos para guardar. Agrega al menos un "
                           "depósito o una tienda.");
      return;
    }

    QString ruta = QFileDialog::getSaveFileName(
        &mainWindow, "Guardar sesión VRP", QDir::homePath() + "/sesion_vrp.vrp",
        "Sesión VRP (*.vrp);;JSON (*.json)");
    if (ruta.isEmpty())
      return;

    // ── Construir JSON ────────────────────────────────────────────────
    QJsonObject root;
    root["version"] = 1;

    // Depósito (primer nodo con esDeposito == true)
    QJsonObject jDeposito;
    bool hayDeposito = false;
    for (const auto &nd : bridge->listaNodos) {
      if (nd.esDeposito) {
        jDeposito["nombre"] = QString::fromStdString(nd.nombre);
        jDeposito["lat"] = static_cast<double>(nd.pos_y);
        jDeposito["lng"] = static_cast<double>(nd.pos_x);
        hayDeposito = true;
        break;
      }
    }
    if (hayDeposito)
      root["deposito"] = jDeposito;

    // Tiendas (todos los nodos que NO son depósito)
    QJsonArray jTiendas;
    for (const auto &nd : bridge->listaNodos) {
      if (nd.esDeposito)
        continue;
      QJsonObject jT;
      jT["nombre"] = QString::fromStdString(nd.nombre);
      jT["lat"] = static_cast<double>(nd.pos_y);
      jT["lng"] = static_cast<double>(nd.pos_x);
      jT["demanda"] = static_cast<double>(nd.demanda);
      jTiendas.append(jT);
    }
    root["tiendas"] = jTiendas;

    // Vehículos
    QJsonArray jVehiculos;
    for (const auto &v : bridge->listaVehiculos) {
      QJsonObject jV;
      jV["placa"] = QString::fromStdString(v.placa);
      jV["capacidad"] = static_cast<double>(v.capacidad);
      jVehiculos.append(jV);
    }
    root["vehiculos"] = jVehiculos;

    // ── Escribir archivo ──────────────────────────────────────────────
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::critical(&mainWindow, "Error",
                            "No se pudo crear el archivo:\n" + ruta);
      return;
    }
    archivo.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    archivo.close();

    int nTiendas = static_cast<int>(jTiendas.count());
    int nVehiculos = static_cast<int>(jVehiculos.count());
    QMessageBox::information(
        &mainWindow, "Sesión guardada",
        QString("Archivo guardado correctamente.\n\n"
                "Depósito: %1\n"
                "Tiendas guardadas: %2\n"
                "Vehículos guardados: %3\n\n"
                "Ruta: %4")
            .arg(hayDeposito
                     ? QString::fromStdString(bridge->listaNodos[0].nombre)
                     : "(ninguno)")
            .arg(nTiendas)
            .arg(nVehiculos)
            .arg(ruta));
  });

  // ── Cargar sesión ─────────────────────────────────────────────────────
  QObject::connect(btnCargar, &QPushButton::clicked, [&]() {
    QString ruta = QFileDialog::getOpenFileName(
        &mainWindow, "Cargar sesión VRP", QDir::homePath(),
        "Sesión VRP (*.vrp);;JSON (*.json);;Todos (*.*)");
    if (ruta.isEmpty())
      return;

    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QMessageBox::critical(&mainWindow, "Error",
                            "No se pudo abrir el archivo:\n" + ruta);
      return;
    }
    QByteArray datos = archivo.readAll();
    archivo.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(datos, &err);
    if (doc.isNull() || !doc.isObject()) {
      QMessageBox::critical(&mainWindow, "Error de formato",
                            "El archivo no es un JSON válido:\n" +
                                err.errorString());
      return;
    }

    QJsonObject root = doc.object();
    if (root["version"].toInt() != 1) {
      QMessageBox::warning(
          &mainWindow, "Versión desconocida",
          "El archivo fue guardado con una versión distinta del programa.\
 Se intentará cargar de todas formas.");
    }

    // ── Limpiar estado actual ─────────────────────────────────────────
    bridge->listaNodos.clear();
    bridge->listaVehiculos.clear();
    listaVehiculosUI->clear();
    listaTiendasUI->clear();
    contTienda = 1;
    totalDemandaAcumulada = 0.0;
    lblDemandaTotal->setText("Demanda total del viaje: 0");

    // Limpiar marcadores y rutas en el mapa
    mapView->page()->runJavaScript(
        "if (typeof limpiarMarcadores === 'function') limpiarMarcadores();");

    // ── Restaurar depósito ────────────────────────────────────────────
    if (root.contains("deposito") && root["deposito"].isObject()) {
      QJsonObject jDep = root["deposito"].toObject();
      QString depNombre = jDep["nombre"].toString("Depósito Central");
      double depLat = jDep["lat"].toDouble();
      double depLng = jDep["lng"].toDouble();

      // Reconstruir nodo en el bridge
      Nodo dep;
      dep.id = 0;
      dep.grafoNodeId = 0;
      dep.nombre = depNombre.toStdString();
      dep.pos_x = static_cast<float>(depLng);
      dep.pos_y = static_cast<float>(depLat);
      dep.esDeposito = true;
      dep.demanda = 0.f;
      bridge->listaNodos.push_back(dep);

      // Dibujar marcador en el mapa en la posición exacta guardada
      QString depNombreEscapado = depNombre;
      depNombreEscapado.replace("'", "\\'");
      mapView->page()->runJavaScript(
          QString("agregarMarcadorDepositoEnPos('%1', %2, %3);")
              .arg(depNombreEscapado)
              .arg(depLat, 0, 'f', 8)
              .arg(depLng, 0, 'f', 8));
    }

    // ── Restaurar tiendas ─────────────────────────────────────────────
    if (root.contains("tiendas") && root["tiendas"].isArray()) {
      QJsonArray jTiendas = root["tiendas"].toArray();
      for (const QJsonValue &val : jTiendas) {
        if (!val.isObject())
          continue;
        QJsonObject jT = val.toObject();
        QString nombre =
            jT["nombre"].toString(QString("Tienda %1").arg(contTienda));
        double lat = jT["lat"].toDouble();
        double lng = jT["lng"].toDouble();
        double demanda = jT["demanda"].toDouble();

        // Reconstruir nodo en el bridge
        Nodo nd;
        nd.id = static_cast<int>(bridge->listaNodos.size());
        nd.grafoNodeId = nd.id;
        nd.nombre = nombre.toStdString();
        nd.pos_x = static_cast<float>(lng);
        nd.pos_y = static_cast<float>(lat);
        nd.esDeposito = false;
        nd.demanda = static_cast<float>(demanda);
        bridge->listaNodos.push_back(nd);

        totalDemandaAcumulada += demanda;
        contTienda++;

        // Agregar a lista visual de tiendas
        QListWidgetItem *tItem = new QListWidgetItem(
            QString("%1  |  Dem: %2").arg(nombre).arg(demanda));
        tItem->setData(Qt::UserRole, nombre);
        listaTiendasUI->addItem(tItem);

        // Dibujar marcador en el mapa en la posición exacta guardada
        QString nombreEscapado = nombre;
        nombreEscapado.replace("'", "\\'");
        mapView->page()->runJavaScript(
            QString("agregarMarcadorClienteEnPos('%1', %2, %3, %4);")
                .arg(nombreEscapado)
                .arg(demanda, 0, 'f', 4)
                .arg(lat, 0, 'f', 8)
                .arg(lng, 0, 'f', 8));
      }
    }

    // ── Restaurar vehículos ───────────────────────────────────────────
    if (root.contains("vehiculos") && root["vehiculos"].isArray()) {
      QJsonArray jVehiculos = root["vehiculos"].toArray();
      for (const QJsonValue &val : jVehiculos) {
        if (!val.isObject())
          continue;
        QJsonObject jV = val.toObject();
        QString placa = jV["placa"].toString();
        float capacidad = static_cast<float>(jV["capacidad"].toDouble());

        Vehiculo v;
        v.placa = placa.toStdString();
        v.capacidad = capacidad;
        bridge->listaVehiculos.push_back(v);

        int vIdx = static_cast<int>(bridge->listaVehiculos.size()) - 1;
        listaVehiculosUI->addItem(
            QString("V%1  |  %2  |  Cap: %3").arg(vIdx).arg(placa).arg(capacidad));
      }
    }

    // Actualizar etiqueta de demanda total
    lblDemandaTotal->setText(
        QString("Demanda total del viaje: %1").arg(totalDemandaAcumulada));

    int nTiendas = static_cast<int>(bridge->listaNodos.size()) -
                   (bridge->listaNodos.empty()         ? 0
                    : bridge->listaNodos[0].esDeposito ? 1
                                                       : 0);
    int nVehiculos = static_cast<int>(bridge->listaVehiculos.size());
    QMessageBox::information(&mainWindow, "Sesión cargada",
                             QString("Sesión restaurada correctamente.\n\n"
                                     "Tiendas cargadas: %1\n"
                                     "Vehículos cargados: %2")
                                 .arg(nTiendas)
                                 .arg(nVehiculos));
  });

  // Lógica principal: Calcular la ruta
  QObject::connect(btnCalcular, &QPushButton::clicked, [=, &mainWindow]() {
    // ── Validación previa: demanda total vs. capacidad total de la flota
    // ────────
    float demandaTotal = 0.f;
    float capacidadTotal = 0.f;

    for (const auto &n : bridge->listaNodos)
      if (!n.esDeposito)
        demandaTotal += n.cargaEfectiva();

    for (const auto &v : bridge->listaVehiculos)
      capacidadTotal += v.capacidad;

    if (bridge->listaNodos.empty() || bridge->listaVehiculos.empty()) {
      QMessageBox::warning(&mainWindow, "Datos incompletos",
                           "Debes agregar al menos un depósito, un vehículo y "
                           "una tienda antes de calcular.");
      return;
    }
    if (demandaTotal > capacidadTotal) {
      QMessageBox::warning(
          &mainWindow, "Error de Capacidad",
          QString("No se puede realizar la ruta por falta de vehículos.\n\n"
                  "Demanda total: %1\n"
                  "Capacidad total de la flota: %2\n\n"
                  "Agrega más vehículos o reduce las demandas.")
              .arg(demandaTotal)
              .arg(capacidadTotal));
      return;
    }

    // 1. Limpiar rutas anteriores: resetea capas, grupos y el control de capas
    //    Se llama limpiarRutasPantalla() que ya itera rutasPorVehiculo y
    //    hace controlCapas.removeLayer() + map.removeLayer() en cada grupo.
    mapView->page()->runJavaScript("if (typeof limpiarRutasPantalla === "
                                   "'function') limpiarRutasPantalla();");

    // 2. Calcular la MatrizDist (float, firma real de los algoritmos)
    MatrizDist matriz = bridge->calcularMatrizDistancias();
    QString alg = comboAlgoritmo->currentText();
    ResultadoAlgoritmo res;

    // ── Advertencia educativa para Fuerza Bruta: mostrar O(n!) ──────────
    if (alg == "Fuerza Bruta") {
      int nCli = 0;
      for (const auto &nd : bridge->listaNodos)
        if (!nd.esDeposito)
          ++nCli;

      long long factorial = 1;
      bool overflow = false;
      for (int k = 1; k <= nCli; ++k) {
        if (factorial > (long long)2e15 / k) {
          overflow = true;
          break;
        }
        factorial *= k;
      }
      QString permStr = overflow
                            ? QString("&gt; 2 &times; 10<sup>15</sup> "
                                      "<i>(inviable en la pr&aacute;ctica)</i>")
                            : QString("<b>%L1</b>").arg(factorial);

      QString adv =
          QString(
              "<h3 style='color:#c0392b; margin:0 0 8px 0;'>"
              "&#9888;&nbsp; Advertencia &mdash; Complejidad "
              "<code>O(n!)</code></h3>"
              "<p>Con <b>%1</b> cliente(s), Fuerza Bruta evaluar&aacute;:</p>"
              "<p style='font-size:15px; text-align:center; color:#c0392b;'>%2 "
              "permutaciones</p>"
              "<table border='1' cellpadding='4' "
              "style='border-collapse:collapse; "
              "font-size:11px; width:100%;'>"
              "<tr style='background:#2c3e50; color:white;'>"
              "<th>n (clientes)</th><th>n! (permutaciones)</th><th>Tiempo "
              "estimado</th></tr>"
              "<tr><td>5</td><td>120</td><td>&lt; 1 ms</td></tr>"
              "<tr><td>8</td><td>40,320</td><td>~1&ndash;10 ms</td></tr>"
              "<tr><td>10</td><td>3,628,800</td><td>~100 ms &ndash; 1 "
              "s</td></tr>"
              "<tr><td>12</td><td>479,001,600</td><td>~minutos</td></tr>"
              "<tr><td>15</td><td>1,307,674,368,000</td><td>d&iacute;as / "
              "a&ntilde;os</td></tr>"
              "</table>"
              "<p style='margin-top:8px;'><b>Esta es la limitaci&oacute;n "
              "fundamental "
              "de Fuerza Bruta.</b><br>"
              "Greedy y Gen&eacute;tico fueron dise&ntilde;ados para superar "
              "exactamente este "
              "problema.</p>")
              .arg(nCli)
              .arg(permStr);

      QMessageBox msgBox(&mainWindow);
      msgBox.setWindowTitle("Complejidad Computacional \u2014 O(n!)");
      msgBox.setText(adv);
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
      msgBox.button(QMessageBox::Yes)->setText("  Continuar de todas formas  ");
      msgBox.button(QMessageBox::Cancel)->setText("  Cancelar  ");
      msgBox.setDefaultButton(QMessageBox::Cancel);
      if (msgBox.exec() != QMessageBox::Yes)
        return;
    }

    // 3. Llamar al algoritmo seleccionado con la firma real: (dist, nodos,
    // vehiculos)
    if (alg == "Greedy") {
      res = ejecutarGreedy(matriz, bridge->listaNodos, bridge->listaVehiculos);
    } else if (alg == "Fuerza Bruta") {
      res = ejecutarFuerzaBruta(matriz, bridge->listaNodos,
                                bridge->listaVehiculos);
    } else {
      res =
          ejecutarGenetico(matriz, bridge->listaNodos, bridge->listaVehiculos);
    }

    // 4. Graficar: ResultadoAlgoritmo.rutas es vector<vector<int>> (índices).
    //    Cada ruta corresponde a un vehículo; se pasa su índice como idVehiculo
    //    para que JS agrupe los segmentos en el Control de Capas correcto.
    QStringList colores = {"#e74c3c", "#3498db", "#2ecc71",
                           "#f39c12", "#9b59b6", "#1abc9c"};

    for (size_t i = 0; i < res.rutas.size(); ++i) {
      QString color = colores[static_cast<int>(i) % colores.size()];
      // Usamos la placa del vehículo correspondiente en lugar del índice
      QString placaVehiculo =
          QString::fromStdString(bridge->listaVehiculos[i].placa);
      const auto &ruta = res.rutas[i]; // vector<int> de índices

      for (size_t j = 0; j + 1 < ruta.size(); ++j) {
        const Nodo &a = bridge->listaNodos[ruta[j]];
        const Nodo &b = bridge->listaNodos[ruta[j + 1]];

        // pos_x = lng, pos_y = lat  (ver recibirCoordenadasCliente)
        double lat1 = static_cast<double>(a.pos_y);
        double lng1 = static_cast<double>(a.pos_x);
        double lat2 = static_cast<double>(b.pos_y);
        double lng2 = static_cast<double>(b.pos_x);

        // Pasar placaVehiculo como 6.° argumento para agrupar en el Control de
        // Capas
        QString jsCode = QString("if (typeof dibujarRutaVial === 'function') "
                                 "dibujarRutaVial(%1, %2, %3, %4, '%5', '%6');")
                             .arg(lat1, 0, 'f', 6)
                             .arg(lng1, 0, 'f', 6)
                             .arg(lat2, 0, 'f', 6)
                             .arg(lng2, 0, 'f', 6)
                             .arg(color)
                             .arg(placaVehiculo);
        mapView->page()->runJavaScript(jsCode);
      }

      // Etiquetar cada parada con su vehiculo y orden: "V{i} | P{orden}"
      int stopOrder = 1;
      for (size_t j = 0; j < ruta.size(); ++j) {
        if (ruta[j] < static_cast<int>(bridge->listaNodos.size()) &&
            !bridge->listaNodos[ruta[j]].esDeposito) {
          QString nNombre =
              QString::fromStdString(bridge->listaNodos[ruta[j]].nombre);
          QString nNombreJS = nNombre;
          nNombreJS.replace("'", "\\'");
          mapView->page()->runJavaScript(
              QString("if (typeof etiquetarParadaTienda === 'function') "
                      "etiquetarParadaTienda('%1', 'V%2', %3);")
                  .arg(nNombreJS)
                  .arg(i)
                  .arg(stopOrder++));
        }
      }
    }

    // ── Resultados: mostrar panel educativo con detalle por ruta ────────
    double distFinal = res.distanciaTotal;
    bool rutaIncompleta = (distFinal >= 1e8);
    int clientesFuera = 0;

    if (rutaIncompleta) {
      clientesFuera = static_cast<int>(distFinal / 1e9);
      distFinal = std::fmod(distFinal, 1e9);
      QMessageBox::warning(&mainWindow, "Problema de Empaquetado (Bin Packing)",
                           QString("No se traz\u00f3 la ruta completa. "
                                   "Quedaron %1 tienda(s) sin asignar.\n\n"
                                   "Soluci\u00f3n: Agrega un veh\u00edculo "
                                   "adicional o aumenta la capacidad.")
                               .arg(clientesFuera));
    }

    // Mapear algoritmo a nombre y complejidad
    QString algNombre, algComplejidad;
    if (alg == "Greedy") {
      algNombre = "Greedy (Nearest Neighbor)";
      algComplejidad = "O(n^2 * V)";
    } else if (alg == "Fuerza Bruta") {
      algNombre = "Fuerza Bruta";
      algComplejidad = "O(n!)";
    } else {
      algNombre = "Algoritmo Genetico";
      algComplejidad = "O(G * P * n)";
    }

    // ── Construir HTML sin emojis ─────────────────────────────────────────
    QString html;
    html += QString("<b style='font-size:13px;'>%1</b><br>").arg(algNombre);
    html += QString("<span style='color:#555; font-size:11px;'>"
                    "Complejidad teorica: <code>%1</code></span><br>")
                .arg(algComplejidad);
    html += "<hr style='border:none; border-top:1px solid #ccc; margin:4px 0;'>";
    html += QString("<b>Distancia Total:</b> %1 m<br>"
                    "<b>Tiempo de Ejecucion:</b> %2 ms<br>")
                .arg(distFinal, 0, 'f', 2)
                .arg(res.tiempoMs, 0, 'f', 4);

    if (rutaIncompleta)
      html += QString("<b style='color:#c0392b;'>AVISO: %1 tienda(s) sin asignar</b><br>")
                  .arg(clientesFuera);

    html += "<hr style='border:none; border-top:1px solid #ccc; margin:4px 0;'>";

    // Detalle por ruta/vehículo
    const auto &nodos = bridge->listaNodos;
    const auto &vehis = bridge->listaVehiculos;
    for (size_t ri = 0; ri < res.rutas.size(); ++ri) {
      const auto &ruta = res.rutas[ri];
      if (ruta.size() < 2)
        continue;

      float cargaUsada = 0.f;
      double distParcial = 0.0;
      for (size_t j = 0; j + 1 < ruta.size(); ++j) {
        distParcial += static_cast<double>(matriz[ruta[j]][ruta[j + 1]]);
        if (!nodos[ruta[j]].esDeposito)
          cargaUsada += nodos[ruta[j]].cargaEfectiva();
      }

      QString placa = (ri < vehis.size())
                          ? QString::fromStdString(vehis[ri].placa)
                          : QString("V%1").arg(ri + 1);
      float capVehi = (ri < vehis.size()) ? vehis[ri].capacidad : 0.f;

      QStringList seq;
      for (int idx : ruta)
        seq << QString::fromStdString(nodos[idx].nombre).toHtmlEscaped();

      html += QString("<b>Vehiculo: %1</b> <span style='color:#555;'>(Cap: %2)</span><br>"
                      "<span style='font-size:10px;'>%3</span><br>"
                      "Carga: <b>%4 / %5</b>  |  Distancia: <b>%6 m</b><br><br>")
                  .arg(placa.toHtmlEscaped())
                  .arg(capVehi, 0, 'f', 0)
                  .arg(seq.join(" > "))
                  .arg(cargaUsada, 0, 'f', 1)
                  .arg(capVehi, 0, 'f', 0)
                  .arg(distParcial, 0, 'f', 2);
    }

    resultsBrowser->setHtml(html);
  });

  // ── Comparar Algoritmos: ventana educativa con tabla completa ──────────────
  QObject::connect(btnComparar, &QPushButton::clicked, [&mainWindow, bridge]() {
    if (bridge->listaNodos.empty() || bridge->listaVehiculos.empty()) {
      QMessageBox::warning(&mainWindow, "Datos incompletos",
                           "Debes agregar al menos un dep\u00f3sito, un "
                           "veh\u00edculo y una tienda antes de comparar.");
      return;
    }

    // Contar clientes y vehículos para mostrar en el encabezado
    int nCli = 0, nVehi = static_cast<int>(bridge->listaVehiculos.size());
    for (const auto &nd : bridge->listaNodos)
      if (!nd.esDeposito)
        ++nCli;

    // Advertencia si Fuerza Bruta puede tardar demasiado
    if (nCli > 8) {
      long long fact = 1;
      bool ovf = false;
      for (int k = 1; k <= nCli; ++k) {
        if (fact > (long long)2e15 / k) {
          ovf = true;
          break;
        }
        fact *= k;
      }
      QString factStr = ovf ? QString("m\u00e1s de 2\u00d710\u00b9\u2075")
                            : QString::number(fact);
      int resp = QMessageBox::warning(
          &mainWindow, "Advertencia \u2014 Fuerza Bruta en la Comparaci\u00f3n",
          QString("<b>Fuerza Bruta con %1 clientes evaluar\u00e1 %2 "
                  "permutaciones.</b><br><br>"
                  "Esto puede tardar varios segundos o minutos.<br>"
                  "\u00bfDesea continuar con los tres algoritmos?")
              .arg(nCli)
              .arg(factStr),
          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
      if (resp != QMessageBox::Yes)
        return;
    }

    MatrizDist matriz = bridge->calcularMatrizDistancias();

    // Ejecutar los tres algoritmos
    ResultadoAlgoritmo resG =
        ejecutarGreedy(matriz, bridge->listaNodos, bridge->listaVehiculos);
    ResultadoAlgoritmo resGEN =
        ejecutarGenetico(matriz, bridge->listaNodos, bridge->listaVehiculos);
    ResultadoAlgoritmo resFB =
        ejecutarFuerzaBruta(matriz, bridge->listaNodos, bridge->listaVehiculos);

    // Extraer distancias reales (sin el factor de penalización 1e9)
    double dG = resG.distanciaTotal, dGEN = resGEN.distanciaTotal,
           dFB = resFB.distanciaTotal;
    bool incG = (dG >= 1e8), incGEN = (dGEN >= 1e8), incFB = (dFB >= 1e8);
    if (incG)
      dG = std::fmod(dG, 1e9);
    if (incGEN)
      dGEN = std::fmod(dGEN, 1e9);
    if (incFB)
      dFB = std::fmod(dFB, 1e9);

    // Mejor distancia entre rutas completas
    double dMin = std::numeric_limits<double>::max();
    if (!incG)
      dMin = std::min(dMin, dG);
    if (!incGEN)
      dMin = std::min(dMin, dGEN);
    if (!incFB)
      dMin = std::min(dMin, dFB);
    if (dMin == std::numeric_limits<double>::max())
      dMin = std::min(std::min(dG, dGEN), dFB); // si todas incompletas

    // ── Crear el diálogo con QTableWidget (escala bien al redimensionar) ──
    QDialog *dlg = new QDialog(&mainWindow);
    dlg->setWindowTitle("Comparacion de Algoritmos - VRP Logistica");
    dlg->setMinimumSize(700, 400);
    dlg->resize(860, 600);
    // Habilitar boton de maximizar en la barra de titulo
    dlg->setWindowFlags(dlg->windowFlags()
                        | Qt::WindowMaximizeButtonHint
                        | Qt::WindowMinimizeButtonHint);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *dlgLayout = new QVBoxLayout(dlg);
    dlgLayout->setContentsMargins(16, 12, 16, 12);
    dlgLayout->setSpacing(8);

    // Título
    QLabel *dlgTitle = new QLabel(
        QString("<b style='font-size:15px;'>Comparacion de Algoritmos - VRP</b>"
                "<br><span style='font-size:11px; color:#555;'>"
                "n = %1 cliente(s)  |  V = %2 vehiculo(s)</span>")
            .arg(nCli).arg(nVehi), dlg);
    dlgTitle->setAlignment(Qt::AlignCenter);
    dlgTitle->setStyleSheet("padding:6px; background:#f0f0f0; border:1px solid #ccc; border-radius:4px;");
    dlgLayout->addWidget(dlgTitle);

    // Tabla de metricas
    QLabel *lblMetricas = new QLabel("<b>Metricas de rendimiento</b>", dlg);
    dlgLayout->addWidget(lblMetricas);

    QTableWidget *tblMetricas = new QTableWidget(3, 5, dlg);
    tblMetricas->setHorizontalHeaderLabels(
        {"Algoritmo", "Complejidad", "Distancia (m)", "Tiempo (ms)", "Calidad vs Mejor"});
    tblMetricas->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tblMetricas->verticalHeader()->setVisible(false);
    tblMetricas->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblMetricas->setSelectionMode(QAbstractItemView::NoSelection);
    tblMetricas->setAlternatingRowColors(false);
    tblMetricas->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { background-color:#34495e; color:white;"
        " font-weight:bold; padding:6px; border:1px solid #2c3e50; }");
    tblMetricas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    tblMetricas->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    tblMetricas->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Colores de fila
    auto rowColor = [&](double d, bool inc) -> QColor {
      if (inc)                          return QColor("#f5c6cb"); // rojo
      if (std::abs(d - dMin) < 0.01)   return QColor("#c3e6cb"); // verde
      return QColor("#ffeeba");                                   // amarillo
    };
    auto qualText = [&](double d, bool inc) -> QString {
      if (inc) return "Ruta incompleta";
      if (dMin > 0 && std::abs(d - dMin) < 0.01)
        return "MEJOR (menor distancia)";
      double pct = (dMin > 0) ? (d / dMin - 1.0) * 100.0 : 0.0;
      return QString("+%1 % sobre el mejor").arg(pct, 0, 'f', 2);
    };

    // Datos de las tres filas
    struct RowData { QString nombre; QString complejidad; double dist; double tms; bool inc; };
    std::vector<RowData> filas = {
      {"Greedy (Nearest Neighbor)",    "O(n^2 * V)", dG,   resG.tiempoMs,   incG  },
      {"Fuerza Bruta (Exhaustiva)",    "O(n!)",      dFB,  resFB.tiempoMs,  incFB },
      {"Algoritmo Genetico",           "O(G*P*n)",   dGEN, resGEN.tiempoMs, incGEN}
    };

    for (int r = 0; r < 3; ++r) {
      const auto &f = filas[r];
      QColor bg = rowColor(f.dist, f.inc);
      auto cell = [&](int col, const QString &txt) {
        QTableWidgetItem *it = new QTableWidgetItem(txt);
        it->setBackground(bg);
        it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        it->setFlags(Qt::ItemIsEnabled);
        tblMetricas->setItem(r, col, it);
      };
      cell(0, f.nombre);
      cell(1, f.complejidad);
      cell(2, f.inc ? QString("%1 (incompleta)").arg(f.dist, 0,'f',2)
                    : QString("%1").arg(f.dist, 0,'f',2));
      cell(3, QString("%1").arg(f.tms, 0,'f',4));
      cell(4, qualText(f.dist, f.inc));
    }
    tblMetricas->resizeRowsToContents();
    dlgLayout->addWidget(tblMetricas);

    // Tabla de ventajas y limitaciones
    QLabel *lblVL = new QLabel("<b>Ventajas y Limitaciones</b>", dlg);
    dlgLayout->addWidget(lblVL);

    QTableWidget *tblVL = new QTableWidget(3, 3, dlg);
    tblVL->setHorizontalHeaderLabels({"Algoritmo", "Ventajas", "Limitaciones"});
    tblVL->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tblVL->verticalHeader()->setVisible(false);
    tblVL->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblVL->setSelectionMode(QAbstractItemView::NoSelection);
    tblVL->setWordWrap(true);
    tblVL->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { background-color:#34495e; color:white;"
        " font-weight:bold; padding:6px; border:1px solid #2c3e50; }");
    tblVL->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    tblVL->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    tblVL->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    struct VLRow { QString alg; QString pros; QString cons; };
    std::vector<VLRow> vlRows = {
      {"Greedy",
       "Muy rapido. Simple de implementar. Escala bien para n grande.",
       "No garantiza el optimo. Resultado depende del punto de inicio. Puede quedar en minimos locales."},
      {"Fuerza Bruta",
       "Garantiza la solucion optima exacta. Determinista (mismo resultado siempre).",
       "Complejidad O(n!): inviable para n > 10-12. Tiempo crece de forma explosiva."},
      {"Genetico",
       "Buenas soluciones para n grande. No requiere explorar todo el espacio. Configurable.",
       "Resultado varia por aleatoriedad. Parametros requieren ajuste. No garantiza optimo global."}
    };

    for (int r = 0; r < 3; ++r) {
      const auto &vl = vlRows[r];
      auto vlCell = [&](int col, const QString &txt) {
        QTableWidgetItem *it = new QTableWidgetItem(txt);
        it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        it->setFlags(Qt::ItemIsEnabled);
        tblVL->setItem(r, col, it);
      };
      vlCell(0, vl.alg);
      vlCell(1, vl.pros);
      vlCell(2, vl.cons);
    }
    tblVL->resizeRowsToContents();
    dlgLayout->addWidget(tblVL);

    // Nota al pie
    QLabel *nota = new QLabel(
        "Calidad: MEJOR = menor distancia encontrada entre los tres algoritmos.  "
        "+X% = su ruta es X% mas larga que la mejor.  "
        "G=500 generaciones, P=100 individuos, n=clientes, V=vehiculos.", dlg);
    nota->setWordWrap(true);
    nota->setStyleSheet("color:#555; font-size:10px; padding:4px;"
                        "background:#f8f8f8; border:1px solid #ddd; border-radius:3px;");
    dlgLayout->addWidget(nota);

    // Botones: Maximizar + Cerrar
    QPushButton *btnMaximizar = new QPushButton("Maximizar", dlg);
    btnMaximizar->setStyleSheet(
        "QPushButton { background-color:#5c6bc0; color:white; font-weight:bold;"
        " padding:8px 20px; border-radius:4px; font-size:12px; }"
        "QPushButton:hover { background-color:#3f51b5; }");
    btnMaximizar->setCursor(Qt::PointingHandCursor);

    QPushButton *btnCerrar = new QPushButton("Cerrar", dlg);
    btnCerrar->setStyleSheet(
        "QPushButton { background-color:#34495e; color:white; font-weight:bold;"
        " padding:8px 28px; border-radius:4px; font-size:12px; }"
        "QPushButton:hover { background-color:#2c3e50; }");
    btnCerrar->setCursor(Qt::PointingHandCursor);
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(btnMaximizar);
    btnRow->addWidget(btnCerrar);
    btnRow->addStretch();
    dlgLayout->addLayout(btnRow);

    QObject::connect(btnMaximizar, &QPushButton::clicked, dlg, &QDialog::showMaximized);
    QObject::connect(btnCerrar, &QPushButton::clicked, dlg, &QDialog::accept);
    dlg->exec();
  });

  mainWindow.show();

  return app.exec();
}

// Requerido por MOC ya que hemos declarado una clase con Q_OBJECT dentro del
// archivo .cpp
#include "main.moc"