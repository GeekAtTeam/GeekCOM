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

if(WIN32)
    list(APPEND SOURCES resources/GeekCOM.rc)
endif()

set(HEADERS
    src/MainWindow.h
    src/SerialDebugWidget.h
    src/SerialTerminalWidget.h
    src/SerialManager.h
    src/HexUtils.h
    src/SerialBaudRates.h
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

# macOS bundle + Dock / Finder 图标（构建机需有 iconutil，通常仅在 macOS 上可用）
if(APPLE)
    set_target_properties(GeekCOM PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "GeekCOM"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )

    find_program(ICONUTIL iconutil)
    find_program(IMAGEMAGICK_CONVERT NAMES convert magick)
    set(GEEKCOM_ICONSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/GeekCOM.iconset")
    set(GEEKCOM_ICNS "${CMAKE_CURRENT_BINARY_DIR}/GeekCOM.icns")

    if(ICONUTIL AND IMAGEMAGICK_CONVERT)
        add_custom_command(
            OUTPUT ${GEEKCOM_ICNS}
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${GEEKCOM_ICONSET_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${GEEKCOM_ICONSET_DIR}
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 16x16     ${GEEKCOM_ICONSET_DIR}/icon_16x16.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 32x32     ${GEEKCOM_ICONSET_DIR}/icon_16x16@2x.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 32x32     ${GEEKCOM_ICONSET_DIR}/icon_32x32.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 64x64     ${GEEKCOM_ICONSET_DIR}/icon_32x32@2x.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 128x128   ${GEEKCOM_ICONSET_DIR}/icon_128x128.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 256x256   ${GEEKCOM_ICONSET_DIR}/icon_128x128@2x.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 256x256   ${GEEKCOM_ICONSET_DIR}/icon_256x256.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 512x512   ${GEEKCOM_ICONSET_DIR}/icon_256x256@2x.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 512x512   ${GEEKCOM_ICONSET_DIR}/icon_512x512.png
            COMMAND ${IMAGEMAGICK_CONVERT} ${CMAKE_SOURCE_DIR}/logo.png -resize 1024x1024 ${GEEKCOM_ICONSET_DIR}/icon_512x512@2x.png
            COMMAND ${ICONUTIL} -c icns ${GEEKCOM_ICONSET_DIR} -o ${GEEKCOM_ICNS}
            DEPENDS ${CMAKE_SOURCE_DIR}/logo.png
            COMMENT "Generating GeekCOM.icns for macOS bundle"
            VERBATIM
        )
        target_sources(GeekCOM PRIVATE ${GEEKCOM_ICNS})
        set_source_files_properties(${GEEKCOM_ICNS} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
        )
        set_target_properties(GeekCOM PROPERTIES
            MACOSX_BUNDLE_ICON_FILE GeekCOM.icns
        )
    else()
        message(WARNING "GeekCOM: iconutil or ImageMagick not found; macOS bundle will not include GeekCOM.icns")
    endif()
endif()

# Windows app (no console)
if(WIN32)
    set_target_properties(GeekCOM PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
