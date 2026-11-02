#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace symctrl;
    try {
        const auto b = fixtures::thermostat();
        const auto& m = b.metadata;
        std::cout << "contract=" << m.contract_version << " schema=" << m.schema_version
                  << " model=" << m.model_id << '\n';
        std::cout << "provenance=" << toString(m.provenance) << " numeric=" << toString(m.numeric_guarantee) << '\n';
        std::cout << "dimensions=" << m.state_dimension << ',' << m.input_dimension << ',' << m.disturbance_dimension
                  << " M=" << m.internal_states << " A=" << m.input_count << " OUT=" << m.outside_state_id << '\n';
        for (StateId s : {StateId{1},StateId{3},kOutside}) {
            const auto& set = b.transitions->successors(s,0);
            std::cout << "successors(" << s << ",0)=";
            std::cout << (set.kind == SuccessorKind::AllStates ? "all:" : "explicit:");
            forEachSuccessor(set,m.internal_states,[](StateId id){ std::cout << ' ' << id; });
            std::cout << '\n';
        }
        const auto bounds = b.grid->cellBounds(2);
        std::cout << "cell(2)=[" << bounds.lo[0] << ',' << bounds.hi[0] << "] input(1)=" << b.inputs->value(1)[0] << '\n';
        const auto& out = b.transitions->successors(0,1);
        std::cout << "OUT nonempty=" << nonempty(out,3) << " contains_OUT=" << contains(out,0,3) << '\n';
        std::cout << "fixture only; no generation or synthesis\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
