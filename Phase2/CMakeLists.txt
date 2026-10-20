cmake_minimum_required(VERSION 3.16)
project(DetectivesDossier CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

include_directories(src)
add_executable(game src/main.cpp)
add_executable(engine_tests tests/tests.cpp)
enable_testing()
target_compile_definitions(engine_tests PRIVATE ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME engine_tests COMMAND engine_tests)

# ---- Qt GUI (built only if Qt 6 is found) ----
find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  add_executable(dossier_gui
    src/gui/main_gui.cpp
    src/gui/GameWidget.cpp
    src/gui/GameWidget.h)
  target_compile_definitions(dossier_gui PRIVATE ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
  target_link_libraries(dossier_gui PRIVATE Qt6::Widgets)
else()
  message(STATUS "Qt6 not found: building console game and tests only")
endif()
