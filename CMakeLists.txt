cmake_minimum_required(VERSION 3.21)
project(symctrl VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(symctrl_contracts INTERFACE)
target_include_directories(symctrl_contracts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(symctrl_contracts INTERFACE cxx_std_17)

add_library(symctrl_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(symctrl_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_subdirectory(src/common)
add_subdirectory(src/discretization)
add_subdirectory(src/abstraction)
add_subdirectory(src/models)
add_subdirectory(src/synthesis)

# Consumers link this one target to use the complete pipeline.
add_library(symctrl INTERFACE)
target_link_libraries(symctrl INTERFACE symctrl_discretization symctrl_abstraction
    symctrl_models symctrl_synthesis)
add_library(symctrl::symctrl ALIAS symctrl)

add_executable(symctrl_cli app/symbolic_control.cpp)
target_link_libraries(symctrl_cli PRIVATE symctrl symctrl_warnings)

# Moteur applicatif du robot (partage entre CLI et Dashboard)
add_library(robot_core
    app/unicycle_dynamics.cpp
    app/mission_compiler.cpp
    app/robot_simulation.cpp
)
target_include_directories(robot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(robot_core PUBLIC symctrl PRIVATE symctrl_warnings)

add_executable(robot_cli app/robot_cli.cpp)
target_link_libraries(robot_cli PRIVATE robot_core symctrl_warnings)

# ---------------------------------------------------------------------------
# Dependances graphiques du Dashboard (GLFW + Dear ImGui + ImPlot)
# ---------------------------------------------------------------------------
include(FetchContent)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw imgui implot)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_library(implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

# Executable Dashboard C++
add_executable(robot_dashboard
    app/dashboard/dashboard_state.cpp
    app/dashboard/dashboard_ui.cpp
    app/dashboard/main_dashboard.cpp
)
target_include_directories(robot_dashboard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/dashboard)
target_link_libraries(robot_dashboard PRIVATE
    robot_core
    imgui
    implot
    Threads::Threads
    symctrl_warnings
)
if(WIN32)
    target_compile_definitions(robot_dashboard PRIVATE NOMINMAX)
endif()