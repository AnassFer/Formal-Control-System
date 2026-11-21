cmake_minimum_required(VERSION 3.21)
project(symctrl VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

option(SYMCTRL_BUILD_CLI "Build the core symbolic-control CLI (symctrl_cli)" ON)
option(SYMCTRL_BUILD_ROBOT "Build the standalone mobile-robot package (robot_cli, robot_dashboard)" ON)
option(SYMCTRL_BUILD_CHALLENGE "Build the standalone circular-building challenge package (challenge_cli, challenge_ui)" ON)
option(SYMCTRL_BUILD_UI "Build graphical UI targets for enabled packages" ON)

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

# Consumers link this one target to use the complete core pipeline.
add_library(symctrl INTERFACE)
target_link_libraries(symctrl INTERFACE symctrl_discretization symctrl_abstraction
    symctrl_models symctrl_synthesis)
add_library(symctrl::symctrl ALIAS symctrl)

if(SYMCTRL_BUILD_CLI)
    add_executable(symctrl_cli app/symbolic_control.cpp)
    target_link_libraries(symctrl_cli PRIVATE symctrl symctrl_warnings)
endif()

if(SYMCTRL_BUILD_ROBOT)
    add_subdirectory(app/robot)
endif()

if(SYMCTRL_BUILD_CHALLENGE)
    add_subdirectory(app/challenge)
endif()
