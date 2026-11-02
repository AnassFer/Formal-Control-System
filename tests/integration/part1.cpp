#include "test.hpp"
#include "oracle.hpp"
#include "symctrl/contracts/factories.hpp"
using namespace symctrl;
void thermostat() {
    const auto grid=makeUniformGrid({{{17},{23}},{3}});
    const auto inputs=makeInputCatalog({{0},{1}});
    const auto dynamics=makeThermostatDynamics();
    const Box disturbance{{10},{15}};
    const auto mono=makeReachability(dynamics,ReachabilityMethod::CooperativeMonotone);
    const auto derivative=makeReachability(dynamics,ReachabilityMethod::BoundedDerivative);
    const auto graph=buildSymbolicModel(*grid,*inputs,*mono,disturbance);
    const auto second=buildSymbolicModel(*grid,*inputs,*derivative,disturbance);
    for(const auto& row:test::thermostat_oracle){
        const auto a=mono->enclose(grid->cellBounds(row.state),inputs->value(row.input),disturbance);
        const auto b=derivative->enclose(grid->cellBounds(row.state),inputs->value(row.input),disturbance);
        CHECK(a.lo.size()==1 && a.hi.size()==1 && b.lo.size()==1 && b.hi.size()==1);
        CHECK(test::near(a.lo[0],row.enclosure.lo[0]) && test::near(a.hi[0],row.enclosure.hi[0]));
        CHECK(test::near(a.lo[0],b.lo[0]) && test::near(a.hi[0],b.hi[0]));
        for(const auto& g:{graph,second}) {const auto& s=g->successors(row.state,row.input);CHECK(s.kind==SuccessorKind::Explicit && s.ids==row.ids);}
    }
    for(const auto& g:{graph,second})for(InputId a=0;a<2;++a){const auto& s=g->successors(0,a);CHECK(s.kind==SuccessorKind::AllStates && s.ids.empty());CHECK(contains(s,0,3));}
    test::SuppliedPolicy policy;
    auto decision=refineDecision(*grid,*inputs,policy,{18});
    CHECK(decision.state==1 && decision.input==1 && decision.physical_input==Vector({1}));
    auto x=dynamics->step({18},decision.physical_input,{10});CHECK(test::near(x[0],19.6) && grid->locate(x)==2);
    CHECK(contains(graph->successors(decision.state,decision.input),grid->locate(x),3));
    decision=refineDecision(*grid,*inputs,policy,x,decision.next_memory);
    CHECK(decision.state==2 && decision.input==0);
    x=dynamics->step(x,decision.physical_input,{10});CHECK(test::near(x[0],18.64) && grid->locate(x)==1);
    CHECK(contains(graph->successors(decision.state,decision.input),grid->locate(x),3));
    for(double initial:{17.0,18.0,19.0,19.6,21.0,22.0,23.0}) {
        Vector current{initial};PolicyMemory memory{};
        for(std::uint64_t t=0;t<24;++t){
            const auto r=refineDecision(*grid,*inputs,policy,current,memory);
            const Vector w{10.0+static_cast<double>((t*3+1)%6)};
            const auto next=dynamics->step(current,r.physical_input,w);
            CHECK(nonempty(graph->successors(r.state,r.input),3));
            CHECK(contains(graph->successors(r.state,r.input),grid->locate(next),3));
            current=next;memory=r.next_memory;
        }
    }
}
int main(int argc,char** argv){return test::run(argc,argv,{{"thermostat",thermostat}},true);}
