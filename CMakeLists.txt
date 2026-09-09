cmake_minimum_required(VERSION 3.20)
project(WipePDF VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.5.3/msvc2019_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

set(MUPDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf")
include_directories("${MUPDF_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
link_directories("${MUPDF_DIR}/lib")

# Core sources
set(CORE_SOURCES
    src/core/PdfDocument.cpp
    src/core/Detectors.cpp
    src/core/Matcher.cpp
    src/core/ContentEdit.cpp
    src/core/WatermarkCleaner.cpp
    src/i18n/I18n.cpp
)

add_library(wipepdf_core STATIC ${CORE_SOURCES})
target_link_libraries(wipepdf_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    "${MUPDF_DIR}/lib/mupdfcpp64.lib"
)

# Test core executable
add_executable(test_core src/test_core.cpp)
target_link_libraries(test_core PRIVATE wipepdf_core)

add_executable(test_img src/test_img.cpp)
target_link_libraries(test_img PRIVATE wipepdf_core)

add_custom_command(TARGET test_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MUPDF_DIR}/bin/mupdfcpp64.dll"
    $<TARGET_FILE_DIR:test_core>
)

# WipePDF GUI Application
set(GUI_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/PdfViewer.cpp
    res/resources.qrc
    res/app.rc
)

add_executable(WipePDF WIN32 ${GUI_SOURCES})
target_link_libraries(WipePDF PRIVATE wipepdf_core)

add_custom_command(TARGET WipePDF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MUPDF_DIR}/bin/mupdfcpp64.dll"
    $<TARGET_FILE_DIR:WipePDF>
)
