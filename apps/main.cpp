#include "symctrl/contracts/factories.hpp"
#include <iostream>
#include <string>
namespace {
class SuppliedPolicy final : public symctrl::ISymbolicPolicy {
public:
    symctrl::PolicyDecision decide(symctrl::PolicyMemory memory, symctrl::StateId state) const override {
        // Manually supplied paper demonstration table; no synthesis.
        if (state < 1 || state > 3) throw std::out_of_range("supplied policy domain");
        return {memory,state == 1 ? symctrl::InputId{1} : symctrl::InputId{0}};
    }
};
}
int main(int argc, char** argv) {
    using namespace symctrl;
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Foundation skeleton; production commands are not implemented.\n"
                  << "thermostat | thermostat-policy | building-n3 | export-thermostat DIRECTORY\n"
                  << "Fixture client: part2_fixture_consumer (separate executable).\n";
        return 0;
    }
    const std::string command = argc > 1 ? argv[1] : "";
    if ((command != "thermostat" && command != "thermostat-policy" && command != "building-n3"
         && command != "export-thermostat") || (command == "export-thermostat" ? argc != 3 : argc != 2)) {
        std::cerr << "usage: symctrl_cli --help\n";
        return 1;
    }
    try {
        const bool building = command == "building-n3";
        const GridSpec spec = building ? GridSpec{{{17,17,17},{22,22,22}}, {10,10,10}}
                                       : GridSpec{{{17},{23}}, {3}};
        auto grid = makeUniformGrid(spec);
        auto inputs = building ? makeCartesianInputs({{0,0.3,0.6},{0,0.3,0.6},{0,0.3,0.6}})
                               : makeInputCatalog({{0},{1}});
        auto dynamics = building ? makeRingBuildingDynamics(3) : makeThermostatDynamics();
        auto reach = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
        const Box disturbance = building ? Box{{},{}} : Box{{10},{15}};
        auto graph = buildSymbolicModel(*grid, *inputs, *reach, disturbance);
        if (command == "thermostat-policy") {
            SuppliedPolicy policy;
            Vector x{18}; PolicyMemory memory{};
            for (int t = 0; t < 2; ++t) {
                const auto decision = refineDecision(*grid, *inputs, policy, x, memory);
                const auto next = dynamics->step(x,decision.physical_input,{10});
                std::cout << "state=" << decision.state << " action=" << decision.input
                          << " u=" << decision.physical_input.at(0) << " x=" << x.at(0)
                          << " w=10 x_next=" << next.at(0) << '\n';
                x=next; memory=decision.next_memory;
            }
        }
        if (command == "export-thermostat") {
            ModelMetadata m;
            m.model_id = "thermostat"; m.source = "paper S7467 section 1";
            m.provenance = Provenance::GeneratedPart1;
            m.model_parameters = {{"a",0.1},{"b",0.2},{"T",30}};
            m.state_dimension = 1; m.input_dimension = 1; m.disturbance_dimension = 1;
            m.internal_states = 3; m.input_count = 2; m.grid = spec; m.disturbance = disturbance;
            m.numeric_guarantee = graph->numericGuarantee();
            exportModelPackage(assembleModelBundle(grid, inputs, graph, m), argv[2]);
        }
        const auto& stats=graph->statistics();
        std::cout << "model=" << (building ? "ring_building N=3 alpha=0.45 beta=0.045 gamma=0.09 Te=-1 Th=50 X=[17,22]^3 cells=10^3 inputs={0,0.3,0.6}^3 W=empty"
                                           : "thermostat a=0.1 b=0.2 T=30 X=[17,23] cells=3 inputs={0,1} W=[10,15]") << '\n'
                  << "numeric=" << toString(graph->numericGuarantee())
                  << " M=" << graph->internalStateCount() << " A=" << graph->inputCount()
                  << " OUT=0 dimension_0_fastest closed_intersections outside=all_states\n"
                  << "processed_pairs=" << stats.processed_pairs << " stored_edges=" << stats.stored_edges
                  << " abstraction_seconds=" << stats.abstraction_seconds << " peak_rss=unavailable\n";
        return 0;
    } catch (const NotImplemented& e) { std::cerr << e.what() << '\n'; return 2; }
      catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
