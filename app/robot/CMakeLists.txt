cmake_minimum_required(VERSION 3.21)

# Can be configured either standalone (`cmake -S app/robot -B build-robot`)
# or via the root repository (`-DSYMCTRL_BUILD_ROBOT=ON -DSYMCTRL_BUILD_CHALLENGE=OFF`).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(symctrl_robot_package VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_EXTENSIONS OFF)
    option(SYMCTRL_BUILD_UI "Build graphical dashboard for the robot package" ON)

    get_filename_component(SYMCTRL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(SYMCTRL_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(SYMCTRL_BUILD_ROBOT OFF CACHE BOOL "" FORCE)
    set(SYMCTRL_BUILD_CHALLENGE OFF CACHE BOOL "" FORCE)
    add_subdirectory("${SYMCTRL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/symctrl-core")
else()
    get_filename_component(SYMCTRL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

# Mobile-robot application core (shared between CLI and Dashboard)
add_library(robot_core
    unicycle_dynamics.cpp
    mission_compiler.cpp
    robot_simulation.cpp
)
target_include_directories(robot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(robot_core PUBLIC symctrl::symctrl PRIVATE symctrl_warnings)

add_executable(robot_cli robot_cli.cpp)
target_link_libraries(robot_cli PRIVATE robot_core symctrl_warnings)

set(ROBOT_PACKAGE_TARGETS robot_cli)

if(SYMCTRL_BUILD_UI)
    include("${SYMCTRL_ROOT}/cmake/symctrl_ui_deps.cmake")

    add_executable(robot_dashboard
        dashboard/dashboard_state.cpp
        dashboard/dashboard_ui.cpp
        dashboard/main_dashboard.cpp
    )
    target_include_directories(robot_dashboard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dashboard)
    target_link_libraries(robot_dashboard PRIVATE
        robot_core
        imgui
        implot
        Threads::Threads
        symctrl_warnings
    )
    if(WIN32)
        target_compile_definitions(robot_dashboard PRIVATE NOMINMAX)
    elseif(APPLE)
        target_compile_definitions(robot_dashboard PRIVATE GLFW_INCLUDE_GLCOREARB GL_SILENCE_DEPRECATION)
    endif()

    list(APPEND ROBOT_PACKAGE_TARGETS robot_dashboard)
endif()

add_custom_target(robot_package DEPENDS ${ROBOT_PACKAGE_TARGETS})
