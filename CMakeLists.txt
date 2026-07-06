cmake_minimum_required(VERSION 3.16)

project(VRP_Logistica VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)

set(PROJECT_SOURCES
    src/main.cpp
    src/FuerzaBruta.cpp
    src/Greedy.cpp
    src/Genetico.cpp
)

add_executable(VRP_Logistica ${PROJECT_SOURCES})

target_link_libraries(VRP_Logistica PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

add_custom_command(TARGET VRP_Logistica POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/mapa.html"
    "$<TARGET_FILE_DIR:VRP_Logistica>/mapa.html"
    COMMENT "Copiando resources/mapa.html al directorio de salida..."
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)