add_library(symctrl_synthesis robust_predecessor.cpp safety_controller.cpp
    reachability_controller.cpp finite_automaton.cpp product_model.cpp automaton_policy.cpp)
target_link_libraries(symctrl_synthesis PUBLIC symctrl_common PRIVATE symctrl_warnings)
