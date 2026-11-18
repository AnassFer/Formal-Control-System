cmake_minimum_required(VERSION 3.21)
project(symctrl VERSION 1.0.0 LANGUAGES CXX)

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
