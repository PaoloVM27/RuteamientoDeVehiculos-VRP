#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QObject>
#include <QString>
#include <cmath>
#include <QWebEngineSettings>
#include <QFile>
#include <QListWidget>
#include <QMessageBox>

// Headers propios del proyecto — fuente única de verdad para las estructuras
#include "Estructuras.hpp"
#include "Algoritmos.hpp"

// ==========================================
// Clase VrpBridge (Comunicación Qt <-> JS)
// ==========================================

class VrpBridge : public QObject {
    Q_OBJECT
public:
    explicit VrpBridge(QObject *parent = nullptr) : QObject(parent) {}

    std::vector<Nodo>     listaNodos;      // Nodo de Estructuras.hpp
    std::vector<Vehiculo> listaVehiculos;  // Vehiculo de Estructuras.hpp

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
        dep.pos_x       = static_cast<float>(lng);
        dep.pos_y       = static_cast<float>(lat);
        dep.nombre      = nombre.toStdString();
        dep.esDeposito  = true;
        dep.demanda     = 0.f;

        if (!listaNodos.empty() && listaNodos[0].esDeposito) {
            dep.id          = 0;
            dep.grafoNodeId = 0;
            listaNodos[0]   = dep;
        } else {
            dep.id          = 0;
            dep.grafoNodeId = 0;
            listaNodos.insert(listaNodos.begin(), dep);
            for (int i = 1; i < static_cast<int>(listaNodos.size()); ++i) {
                listaNodos[i].id          = i;
                listaNodos[i].grafoNodeId = i;
            }
        }
    }

    // ── Invocado por JS al agregar un cliente (tienda/punto de entrega) ──────
    Q_INVOKABLE void recibirCoordenadasCliente(double lat, double lng, QString nombre, double demanda) {
        Nodo n;
        n.id          = static_cast<int>(listaNodos.size());
        n.pos_x       = static_cast<float>(lng);
        n.pos_y       = static_cast<float>(lat);
        n.nombre      = nombre.toStdString();
        n.esDeposito  = false;
        n.demanda     = static_cast<float>(demanda);
        n.grafoNodeId = n.id;
        listaNodos.push_back(n);
    }

    // ── Invocado por JS cuando el usuario arrastra un marcador existente ──────
    // Busca el nodo por nombre y actualiza sus coordenadas.
    Q_INVOKABLE void actualizarPosicionNodo(QString nombre, double lat, double lng) {
        std::string stdNombre = nombre.toStdString();
        for (auto& n : listaNodos) {
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
    QPushButton *btnAgregarDeposito = new QPushButton("Agregar Depósito", leftPanel);
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
    QPushButton *btnAgregarVehiculo = new QPushButton("Agregar Vehículo", leftPanel);
    vehiculoLayout->addWidget(txtPlaca);
    vehiculoLayout->addWidget(txtCapacidad);
    vehiculoLayout->addWidget(btnAgregarVehiculo);
    leftLayout->addLayout(vehiculoLayout);

    // Lista visual de vehículos registrados
    QListWidget *listaVehiculosUI = new QListWidget(leftPanel);
    listaVehiculosUI->setMaximumHeight(90);   // muestra ~3 ítems sin crecer demasiado
    listaVehiculosUI->setFocusPolicy(Qt::NoFocus);
    leftLayout->addWidget(listaVehiculosUI);

    leftLayout->addSpacing(10);

    // Sección Tiendas
    QLabel *lblTienda = new QLabel("<b>Agregar Tienda</b>", leftPanel);
    leftLayout->addWidget(lblTienda);

    QHBoxLayout *tiendaLayout = new QHBoxLayout();
    QLineEdit *txtNombreTienda = new QLineEdit(leftPanel);
    txtNombreTienda->setPlaceholderText("Nombre Tienda");
    QLineEdit *txtDemandaTienda = new QLineEdit(leftPanel);
    txtDemandaTienda->setPlaceholderText("Demanda");
    QPushButton *btnAgregarTienda = new QPushButton("Agregar Tienda", leftPanel);
    tiendaLayout->addWidget(txtNombreTienda);
    tiendaLayout->addWidget(txtDemandaTienda);
    tiendaLayout->addWidget(btnAgregarTienda);
    leftLayout->addLayout(tiendaLayout);

    QLabel *lblDemandaTotal = new QLabel("Demanda total del viaje: 0", leftPanel);
    leftLayout->addWidget(lblDemandaTotal);

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
    btnCalcular->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   font-weight: bold;"
        "   padding: 15px;"
        "   border-radius: 5px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}"
    );
    btnCalcular->setCursor(Qt::PointingHandCursor);
    leftLayout->addWidget(btnCalcular);

    // Botón Comparar Algoritmos
    QPushButton *btnComparar = new QPushButton("COMPARAR ALGORITMOS", leftPanel);
    btnComparar->setStyleSheet(
        "QPushButton {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   font-weight: bold;"
        "   padding: 10px;"
        "   border-radius: 5px;"
        "   font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1e88e5;"
        "}"
    );
    btnComparar->setCursor(Qt::PointingHandCursor);
    leftLayout->addWidget(btnComparar);

    // Etiquetas para mostrar resultados de la última ejecución
    QLabel *lblResultados = new QLabel("<b>Resultados (Última Ejecución)</b>", leftPanel);
    leftLayout->addWidget(lblResultados);

    QLabel *lblDistancia = new QLabel("Distancia Total: -", leftPanel);
    leftLayout->addWidget(lblDistancia);

    QLabel *lblTiempo = new QLabel("Tiempo de Ejecución: -", leftPanel);
    leftLayout->addWidget(lblTiempo);

    leftLayout->addStretch();

    // ==========================================
    // Panel Derecho: Mapa (QWebEngineView)
    // ==========================================
    QWebEngineView *mapView = new QWebEngineView(splitter);
    mapView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    
    // Instanciar nuestro bridge para comunicar C++ y JS
    VrpBridge *bridge = new VrpBridge(&mainWindow);
    
    // Configurar el QWebChannel
    QWebChannel *channel = new QWebChannel(mapView->page());
    channel->registerObject("interfazBridge", bridge);
    mapView->page()->setWebChannel(channel);
    
    // Cargar mapa.html desde el directorio del ejecutable
    // (POST_BUILD en CMakeLists.txt copia resources/mapa.html aquí)
    QString rutaMapa = QCoreApplication::applicationDirPath() + "/mapa.html";
    // Fallback: si no existe, intenta en la subcarpeta resources/ (desarrollo local)
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
        if (nombre.isEmpty()) nombre = "Depósito Central";  // nombre por defecto
        nombre.replace("'", "\\'");
        mapView->page()->runJavaScript(
            QString("agregarMarcadorDeposito('%1');").arg(nombre));
        txtNombreDeposito->clear();
    });

    // Botón Agregar Vehículo → registra en C++ Y muestra en la lista visual
    QObject::connect(btnAgregarVehiculo, &QPushButton::clicked, [=]() {
        QString placa    = txtPlaca->text().trimmed();
        float   capacidad = txtCapacidad->text().toFloat();
        if (placa.isEmpty()) placa = QString("V%1").arg(bridge->listaVehiculos.size() + 1);
        Vehiculo v;
        v.placa     = placa.toStdString();
        v.capacidad = capacidad;
        bridge->listaVehiculos.push_back(v);
        // Mostrar en la lista visual del panel izquierdo
        listaVehiculosUI->addItem(
            QString("  %1  |  Cap: %2").arg(placa).arg(capacidad));
        txtPlaca->clear();
        txtCapacidad->clear();
    });

    // Botón Agregar Tienda → crea marcador draggable en el centro del mapa
    // Usamos un contador estático para generar nombres únicos por defecto
    static int contTienda = 1;
    static double totalDemandaAcumulada = 0;
    QObject::connect(btnAgregarTienda, &QPushButton::clicked, [=]() mutable {
        QString nombre  = txtNombreTienda->text().trimmed();
        double  demanda = txtDemandaTienda->text().toDouble();
        if (nombre.isEmpty()) nombre = QString("Tienda %1").arg(contTienda);
        contTienda++;
        totalDemandaAcumulada += demanda;
        lblDemandaTotal->setText(QString("Demanda total del viaje: %1").arg(totalDemandaAcumulada));
        nombre.replace("'", "\\'");
        mapView->page()->runJavaScript(
            QString("agregarMarcadorCliente('%1', %2);").arg(nombre).arg(demanda));
        txtNombreTienda->clear();
        txtDemandaTienda->clear();
    });

    // Lógica principal: Calcular la ruta
    QObject::connect(btnCalcular, &QPushButton::clicked, [=, &mainWindow]() {
        // ── Validación previa: demanda total vs. capacidad total de la flota ────────
        float demandaTotal   = 0.f;
        float capacidadTotal = 0.f;

        for (const auto& n : bridge->listaNodos)
            if (!n.esDeposito) demandaTotal += n.cargaEfectiva();

        for (const auto& v : bridge->listaVehiculos)
            capacidadTotal += v.capacidad;

        if (bridge->listaNodos.empty() || bridge->listaVehiculos.empty()) {
            QMessageBox::warning(&mainWindow, "Datos incompletos",
                "Debes agregar al menos un depósito, un vehículo y una tienda antes de calcular.");
            return;
        }
        if (demandaTotal > capacidadTotal) {
            QMessageBox::warning(&mainWindow, "Error de Capacidad",
                QString("No se puede realizar la ruta por falta de vehículos.\n\n"
                        "Demanda total: %1\n"
                        "Capacidad total de la flota: %2\n\n"
                        "Agrega más vehículos o reduce las demandas.")
                    .arg(demandaTotal).arg(capacidadTotal));
            return;
        }

        // 1. Limpiar rutas anteriores: resetea capas, grupos y el control de capas
        //    Se llama limpiarRutasPantalla() que ya itera rutasPorVehiculo y
        //    hace controlCapas.removeLayer() + map.removeLayer() en cada grupo.
        mapView->page()->runJavaScript(
            "if (typeof limpiarRutasPantalla === 'function') limpiarRutasPantalla();");

        // 2. Calcular la MatrizDist (float, firma real de los algoritmos)
        MatrizDist matriz = bridge->calcularMatrizDistancias();
        QString    alg    = comboAlgoritmo->currentText();
        ResultadoAlgoritmo res;

        // 3. Llamar al algoritmo seleccionado con la firma real: (dist, nodos, vehiculos)
        if (alg == "Greedy") {
            res = ejecutarGreedy(matriz, bridge->listaNodos, bridge->listaVehiculos);
        } else if (alg == "Fuerza Bruta") {
            res = ejecutarFuerzaBruta(matriz, bridge->listaNodos, bridge->listaVehiculos);
        } else {
            res = ejecutarGenetico(matriz, bridge->listaNodos, bridge->listaVehiculos);
        }

        // 4. Graficar: ResultadoAlgoritmo.rutas es vector<vector<int>> (índices).
        //    Cada ruta corresponde a un vehículo; se pasa su índice como idVehiculo
        //    para que JS agrupe los segmentos en el Control de Capas correcto.
        QStringList colores = {"#e74c3c", "#3498db", "#2ecc71",
                               "#f39c12", "#9b59b6", "#1abc9c"};

        for (size_t i = 0; i < res.rutas.size(); ++i) {
            QString     color      = colores[static_cast<int>(i) % colores.size()];
            // Usamos la placa del vehículo correspondiente en lugar del índice
            QString     placaVehiculo = QString::fromStdString(bridge->listaVehiculos[i].placa);
            const auto& ruta       = res.rutas[i];  // vector<int> de índices

            for (size_t j = 0; j + 1 < ruta.size(); ++j) {
                const Nodo& a = bridge->listaNodos[ruta[j]];
                const Nodo& b = bridge->listaNodos[ruta[j + 1]];

                // pos_x = lng, pos_y = lat  (ver recibirCoordenadasCliente)
                double lat1 = static_cast<double>(a.pos_y);
                double lng1 = static_cast<double>(a.pos_x);
                double lat2 = static_cast<double>(b.pos_y);
                double lng2 = static_cast<double>(b.pos_x);

                // Pasar placaVehiculo como 6.° argumento para agrupar en el Control de Capas
                QString jsCode =
                    QString("if (typeof dibujarRutaVial === 'function') "
                            "dibujarRutaVial(%1, %2, %3, %4, '%5', '%6');")
                    .arg(lat1, 0, 'f', 6)
                    .arg(lng1, 0, 'f', 6)
                    .arg(lat2, 0, 'f', 6)
                    .arg(lng2, 0, 'f', 6)
                    .arg(color)
                    .arg(placaVehiculo);
                mapView->page()->runJavaScript(jsCode);
            }
        }
        
        // Actualizar UI con los resultados
        double distFinal = res.distanciaTotal;
        if (distFinal >= 1e8) {
            // Se restan los 1e9 para mostrar la distancia real de la parte que sí se pudo trazar
            int clientesFuera = static_cast<int>(distFinal / 1e9);
            distFinal = std::fmod(distFinal, 1e9);
            QMessageBox::warning(&mainWindow, "Problema de Empaquetado (Bin Packing)",
                QString("No se trazó la ruta completa. Quedaron %1 tienda(s) sin asignar.\n\n"
                        "¿Por qué pasa esto?\n"
                        "Aunque la suma de todas las demandas es menor a la suma de todas las capacidades, "
                        "el espacio en los vehículos no se puede fraccionar. Es matemáticamente imposible encajar "
                        "estas cajas específicas en estos vehículos específicos.\n\n"
                        "Solución: Agrega un vehículo adicional o aumenta la capacidad de los existentes.")
                .arg(clientesFuera));
        }

        lblDistancia->setText(QString("Distancia Total: %1 m").arg(distFinal, 0, 'f', 2));
        lblTiempo->setText(QString("Tiempo de Ejecución: %1 ms").arg(res.tiempoMs, 0, 'f', 4));
    });

    // Lógica para Comparar Algoritmos
    QObject::connect(btnComparar, &QPushButton::clicked, [&mainWindow, bridge]() {
        if (bridge->listaNodos.empty() || bridge->listaVehiculos.empty()) {
            QMessageBox::warning(&mainWindow, "Datos incompletos",
                "Debes agregar al menos un depósito, un vehículo y una tienda antes de comparar.");
            return;
        }

        MatrizDist matriz = bridge->calcularMatrizDistancias();
        
        // Ejecutamos los tres algoritmos
        ResultadoAlgoritmo resGreedy = ejecutarGreedy(matriz, bridge->listaNodos, bridge->listaVehiculos);
        ResultadoAlgoritmo resGenetico = ejecutarGenetico(matriz, bridge->listaNodos, bridge->listaVehiculos);
        ResultadoAlgoritmo resFB = ejecutarFuerzaBruta(matriz, bridge->listaNodos, bridge->listaVehiculos);
        
        double distG = resGreedy.distanciaTotal;
        double distGen = resGenetico.distanciaTotal;
        double distFB = resFB.distanciaTotal;
        QString avisoG = "", avisoGen = "", avisoFB = "";

        if (distG >= 1e8) {
            distG = std::fmod(distG, 1e9);
            avisoG = " <i style='color:red;'>(Ruta Incompleta)</i>";
        }
        if (distGen >= 1e8) {
            distGen = std::fmod(distGen, 1e9);
            avisoGen = " <i style='color:red;'>(Ruta Incompleta)</i>";
        }
        if (distFB >= 1e8) {
            distFB = std::fmod(distFB, 1e9);
            avisoFB = " <i style='color:red;'>(Ruta Incompleta)</i>";
        }
        
        QString mensaje = QString(
            "<b>Greedy (Nearest Neighbor)</b>%1<br>"
            "Distancia Total: %2 m<br>"
            "Tiempo de Ejecución: %3 ms<br><br>"
            "<b>Fuerza Bruta</b>%4<br>"
            "Distancia Total: %5 m<br>"
            "Tiempo de Ejecución: %6 ms<br><br>"
            "<b>Algoritmo Genético</b>%7<br>"
            "Distancia Total: %8 m<br>"
            "Tiempo de Ejecución: %9 ms"
        ).arg(avisoG).arg(distG, 0, 'f', 2).arg(resGreedy.tiempoMs, 0, 'f', 4)
         .arg(avisoFB).arg(distFB, 0, 'f', 2).arg(resFB.tiempoMs, 0, 'f', 4)
         .arg(avisoGen).arg(distGen, 0, 'f', 2).arg(resGenetico.tiempoMs, 0, 'f', 4);
         
        QMessageBox::information(&mainWindow, "Comparación de Algoritmos", mensaje);
    });

    mainWindow.show();

    return app.exec();
}

// Requerido por MOC ya que hemos declarado una clase con Q_OBJECT dentro del archivo .cpp
#include "main.moc"