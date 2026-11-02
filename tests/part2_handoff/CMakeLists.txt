add_executable(part2_handoff_tests handoff.cpp)
target_link_libraries(part2_handoff_tests PRIVATE symctrl_fixture_support symctrl_test_support)
target_compile_definitions(part2_handoff_tests PRIVATE SYMCTRL_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/fixtures/part1_v1")
foreach(case IN ITEMS thermostat choice consistency packages)
    symctrl_test(handoff.${case} part2_handoff_tests ${case} handoff FALSE)
endforeach()
add_test(NAME handoff.consumer COMMAND ${CMAKE_COMMAND}
    -DCONSUMER=$<TARGET_FILE:part2_fixture_consumer>
    -DEXPECTED=${PROJECT_SOURCE_DIR}/fixtures/part1_v1/thermostat/expected-output.txt
    -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer.cmake)
set_tests_properties(handoff.consumer PROPERTIES LABELS handoff TIMEOUT 20)
