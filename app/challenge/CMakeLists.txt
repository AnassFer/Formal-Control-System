cmake_minimum_required(VERSION 3.21)

# Can be configured either standalone (`cmake -S app/challenge -B build-challenge`)
# or via the root repository (`-DSYMCTRL_BUILD_CHALLENGE=ON -DSYMCTRL_BUILD_ROBOT=OFF`).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(symctrl_challenge_package VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_EXTENSIONS OFF)
    option(SYMCTRL_BUILD_UI "Build graphical UI for the circular-building challenge" ON)

    get_filename_component(SYMCTRL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(SYMCTRL_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(SYMCTRL_BUILD_ROBOT OFF CACHE BOOL "" FORCE)
    set(SYMCTRL_BUILD_CHALLENGE OFF CACHE BOOL "" FORCE)
    add_subdirectory("${SYMCTRL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/symctrl-core")
else()
    get_filename_component(SYMCTRL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

# Circular-building temperature regulation challenge core (shared between CLI and UI)
add_library(challenge_core
    building_challenge.cpp
)
target_include_directories(challenge_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(challenge_core PUBLIC symctrl::symctrl PRIVATE symctrl_warnings)
if(WIN32)
    target_compile_definitions(challenge_core PRIVATE NOMINMAX)
    target_link_libraries(challenge_core PRIVATE psapi)
endif()

add_executable(challenge_cli challenge_cli.cpp)
target_link_libraries(challenge_cli PRIVATE challenge_core symctrl_warnings)

set(CHALLENGE_PACKAGE_TARGETS challenge_cli)

if(SYMCTRL_BUILD_UI)
    include("${SYMCTRL_ROOT}/cmake/symctrl_ui_deps.cmake")

    add_executable(challenge_ui
        ui/challenge_state.cpp
        ui/challenge_ui.cpp
        ui/main_challenge.cpp
    )
    target_include_directories(challenge_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)
    target_link_libraries(challenge_ui PRIVATE
        challenge_core
        imgui
        implot
        Threads::Threads
        symctrl_warnings
    )
    if(WIN32)
        target_compile_definitions(challenge_ui PRIVATE NOMINMAX)
    elseif(APPLE)
        target_compile_definitions(challenge_ui PRIVATE GLFW_INCLUDE_GLCOREARB GL_SILENCE_DEPRECATION)
    endif()

    list(APPEND CHALLENGE_PACKAGE_TARGETS challenge_ui)
endif()

add_custom_target(challenge_package DEPENDS ${CHALLENGE_PACKAGE_TARGETS})
