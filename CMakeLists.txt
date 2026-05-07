cmake_minimum_required(VERSION 3.16)
project(GeekCOM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    SerialPort
)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/SerialDebugWidget.cpp
    src/SerialTerminalWidget.cpp
    src/SerialManager.cpp
    src/HexUtils.cpp
)

set(HEADERS
    src/MainWindow.h
    src/SerialDebugWidget.h
    src/SerialTerminalWidget.h
    src/SerialManager.h
    src/HexUtils.h
)

add_executable(GeekCOM
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

target_link_libraries(GeekCOM PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
)

# macOS bundle
if(APPLE)
    set_target_properties(GeekCOM PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "GeekCOM"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()

# Windows app (no console)
if(WIN32)
    set_target_properties(GeekCOM PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
