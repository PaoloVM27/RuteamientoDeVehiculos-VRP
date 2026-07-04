cmake_minimum_required(VERSION 3.16)

# Nombre del proyecto y lenguaje
project(VRP_Logistica VERSION 1.0 LANGUAGES CXX)

# Estándar de C++ (se recomienda C++17 para Qt 6)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Habilitar extensiones específicas de Qt automáticamente (MOC, UIC, RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Encontrar los paquetes de Qt 6 necesarios
# Requerimos Core, Gui, Widgets y WebEngineWidgets
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)

# Definir los archivos fuente a compilar
set(PROJECT_SOURCES
    src/main.cpp
    src/FuerzaBruta.cpp
    src/Greedy.cpp
    src/Genetico.cpp
)

# Crear el ejecutable
# Nota: Si deseas ocultar la consola en Windows en la versión final, 
# puedes agregar WIN32: add_executable(VRP_Logistica WIN32 ${PROJECT_SOURCES})
add_executable(VRP_Logistica ${PROJECT_SOURCES})

# Vincular las bibliotecas de Qt 6 al ejecutable
target_link_libraries(VRP_Logistica PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

# Comando personalizado para copiar mapa.html al directorio del ejecutable (.exe).
# Utilizamos copy_if_different para evitar copias innecesarias si el archivo no ha cambiado.
# Se ejecuta después de la compilación (POST_BUILD).
add_custom_command(TARGET VRP_Logistica POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/mapa.html"
    "$<TARGET_FILE_DIR:VRP_Logistica>/mapa.html"
    COMMENT "Copiando resources/mapa.html al directorio de salida..."
)

# --- COPIAR RECURSOS AL DIRECTORIO DE SALIDA ---
# Copia la carpeta resources a la ruta de compilación
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
# Si usas MSVC en modo Release, también la copia adentro de la subcarpeta Release
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)