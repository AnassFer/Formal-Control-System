# Reserved. Owners may extend this local file without editing the frozen root.
add_library(symctrl_part2_workspace STATIC
    predecessor.cpp
    ../part2_b/reachability.cpp
    ../part2_b/automaton.cpp
    ../part2_b/product.cpp
    ../part2_b/automaton_policy.cpp
)
target_link_libraries(symctrl_part2_workspace PUBLIC symctrl_common PRIVATE symctrl_warnings)
