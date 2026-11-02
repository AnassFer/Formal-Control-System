#include "test.hpp"
#include "oracle.hpp"
#include "fixture.hpp"
#include <fstream>
#include <sstream>
using namespace symctrl;
void thermostat() {
    auto f=fixtures::thermostat();validateBundle(f);
    CHECK(f.metadata.provenance==Provenance::HandAuthoredFixture);
    CHECK(f.metadata.numeric_guarantee==NumericGuarantee::UncertifiedFloat);
    for(const auto& row:test::thermostat_oracle){const auto& s=f.transitions->successors(row.state,row.input);
        CHECK(s.kind==SuccessorKind::Explicit && s.ids==row.ids);CHECK(nonempty(s,3));}
    for(InputId a=0;a<2;++a){const auto& out=f.transitions->successors(0,a);CHECK(out.kind==SuccessorKind::AllStates && out.ids.empty());
        std::vector<StateId> ids;forEachSuccessor(out,3,[&](StateId id){ids.push_back(id);});CHECK(ids==std::vector<StateId>({0,1,2,3}));}
    CHECK(f.grid->locate({19})==2 && f.grid->locate({23})==3 && f.grid->locate({24})==0);
    CHECK(f.grid->fromIndices({2})==3 && f.grid->indices(2)==GridIndex({1}));
    CHECK(f.grid->intersectingInternalCells({{19},{19}})==std::vector<StateId>({1,2}));
    CHECK(f.inputs->value(0)==Vector({0}) && f.inputs->value(1)==Vector({1}));
    test::throws<std::out_of_range>([&]{f.grid->cellBounds(0);});
    test::throws<std::out_of_range>([&]{f.inputs->value(2);});
    test::throws<std::out_of_range>([&]{f.transitions->successors(4,0);});
}
void choice() {
    auto f=fixtures::nondeterministicChoice();validateBundle(f);
    // Manually checked expectation only. No fixed point, graph search or policy generation.
    CHECK(f.transitions->successors(1,0).ids==std::vector<StateId>({2,3}));
    CHECK(f.transitions->successors(1,1).ids==std::vector<StateId>({2}));
    CHECK(contains(f.transitions->successors(1,0),3,3));
    CHECK(!contains(f.transitions->successors(1,1),3,3));
    CHECK(f.metadata.disturbance_dimension==0 && f.metadata.disturbance.lo.empty());
    CHECK(!nonempty({SuccessorKind::Explicit,{}},3));
}
void consistency() {
    auto f=fixtures::thermostat();auto copy=f;f={};validateBundle(copy);
    CHECK(copy.grid->cellBounds(3).hi==Vector({23}));
    auto broken=copy;broken.metadata.input_count=3;
    test::throws<std::invalid_argument>([&]{validateBundle(broken);});
    broken=copy;broken.metadata.grid.domain.hi={24};test::throws<std::invalid_argument>([&]{validateBundle(broken);});
    broken=copy;broken.metadata.contract_version="2.0.0";test::throws<std::invalid_argument>([&]{validateBundle(broken);});
    broken=copy;broken.metadata.disturbance_dimension=0;test::throws<std::invalid_argument>([&]{validateBundle(broken);});
    broken=copy;broken.metadata.numeric_guarantee=NumericGuarantee::ValidatedEnclosure;test::throws<std::invalid_argument>([&]{validateBundle(broken);});
}
std::string read(const std::string& path){std::ifstream f(path);CHECK(f.good());std::ostringstream s;s<<f.rdbuf();return s.str();}
void packages() {
    const std::string root=SYMCTRL_FIXTURE_ROOT;
    CHECK(read(root+"/thermostat/cells.csv")=="state_id,lower_0,upper_0\n1,17,19\n2,19,21\n3,21,23\n");
    CHECK(read(root+"/thermostat/inputs.csv")=="input_id,u_0\n0,0\n1,1\n");
    std::ostringstream expected;expected<<"state_id,input_id,kind,successor_id\n0,0,all,\n0,1,all,\n";
    for(const auto& row:test::thermostat_oracle)for(StateId id:row.ids)expected<<row.state<<','<<row.input<<",explicit,"<<id<<'\n';
    CHECK(read(root+"/thermostat/transitions.csv")==expected.str());
    CHECK(read(root+"/nondeterministic_choice/transitions.csv")=="state_id,input_id,kind,successor_id\n0,0,all,\n0,1,all,\n1,0,explicit,2\n1,0,explicit,3\n1,1,explicit,2\n2,0,explicit,2\n2,1,explicit,2\n3,0,explicit,3\n3,1,explicit,3\n");
}
int main(int argc,char** argv){return test::run(argc,argv,{{"thermostat",thermostat},{"choice",choice},{"consistency",consistency},{"packages",packages}});}
