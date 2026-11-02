add_library(symctrl_part_b thermostat_dynamics.cpp ring_building_dynamics.cpp
    monotone_reachability.cpp derivative_reachability.cpp explicit_transition_system.cpp
    model_builder.cpp model_export.cpp factories.cpp)
target_link_libraries(symctrl_part_b PUBLIC symctrl_common PRIVATE symctrl_warnings)
