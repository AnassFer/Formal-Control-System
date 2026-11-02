#include "test.hpp"
#include "oracle.hpp"
#include "fixture.hpp"
#include "symctrl/contracts/factories.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
using namespace symctrl;
void equalBox(const Box& actual,const Box& expected) {
    CHECK(actual.lo.size()==expected.lo.size() && actual.hi.size()==expected.hi.size());
    for(std::size_t d=0;d<expected.lo.size();++d){CHECK(test::near(actual.lo[d],expected.lo[d])); CHECK(test::near(actual.hi[d],expected.hi[d]));}
}
void thermostat() {
    auto f=fixtures::thermostat(); auto dynamics=makeThermostatDynamics();
    CHECK(dynamics->stateDimension()==1 && dynamics->inputDimension()==1 && dynamics->disturbanceDimension()==1);
    for(auto method : {ReachabilityMethod::CooperativeMonotone,ReachabilityMethod::BoundedDerivative}) {
        auto r=makeReachability(dynamics,method);
        CHECK(r->numericGuarantee()==NumericGuarantee::UncertifiedFloat);
        for(const auto& row:test::thermostat_oracle) equalBox(r->enclose(f.grid->cellBounds(row.state),f.inputs->value(row.input),{{10},{15}}),row.enclosure);
    }
    CHECK(test::near(dynamics->step({18},{1},{10})[0],19.6));
    CHECK(test::near(dynamics->step({19.6},{0},{10})[0],18.64));
    test::throws<std::invalid_argument>([&]{dynamics->step({18},{1},{});});
}
// Independent analytic test model only. No production dynamics implementation.
class AffineFixture final : public IDynamics {
    bool mixed_,malformed_;
public:
    explicit AffineFixture(bool mixed=false,bool malformed=false):mixed_(mixed),malformed_(malformed){}
    std::size_t stateDimension()const noexcept override{return 2;}
    std::size_t inputDimension()const noexcept override{return 1;}
    std::size_t disturbanceDimension()const noexcept override{return 2;}
    Vector step(const Vector& x,const Vector& u,const Vector& w)const override{
        validateVector(x,2);validateVector(u,1);validateVector(w,2);
        if(mixed_)return {2*x[0]-x[1]-w[0]+2*w[1],-3*x[0]+0.5*x[1]+w[0]-2*w[1]};
        return {2*x[0]+x[1]+w[0],3*x[1]+2*w[0]+w[1]};
    }
    bool cooperativeOn(const Box&,const Vector&,const Box&)const override{return !mixed_;}
    DerivativeBounds absoluteJacobianBounds(const Box&,const Vector&,const Box&)const override{
        if(malformed_)return {{{-1}},{{1}}};
        if(mixed_)return {{{2,1},{3,0.5}},{{1,2},{1,2}}};
        return {{{2,1},{0,3}},{{1,0},{2,1}}};
    }
};
void affine() {
    auto positive=std::make_shared<AffineFixture>(); auto mixed=std::make_shared<AffineFixture>(true);
    const Box states{{1,2},{2,4}},disturbances{{-1,0},{1,2}};
    for(auto method:{ReachabilityMethod::CooperativeMonotone,ReachabilityMethod::BoundedDerivative}) {
        auto r=makeReachability(positive,method); equalBox(r->enclose(states,{0},disturbances),{{3,4},{9,16}});
    }
    auto r=makeReachability(mixed,ReachabilityMethod::BoundedDerivative);
    equalBox(r->enclose(states,{0},disturbances),{{-3,-10},{7,0}});
}
void capabilities() {
    auto mixed=std::make_shared<AffineFixture>(true);
    // Capability errors may be reported at construction or first requested domain.
    test::throws<UnsupportedCapability>([&]{auto r=makeReachability(mixed,ReachabilityMethod::CooperativeMonotone); r->enclose({{1,2},{2,4}},{0},{{-1,0},{1,2}});});
    auto malformed=std::make_shared<AffineFixture>(false,true);
    test::throws<std::invalid_argument>([&]{auto r=makeReachability(malformed,ReachabilityMethod::BoundedDerivative);r->enclose({{1,2},{2,4}},{0},{{-1,0},{1,2}});});
    test::throws<std::invalid_argument>([]{makeReachability(nullptr,ReachabilityMethod::BoundedDerivative);});
    auto d=makeThermostatDynamics();
    test::throws<UnsupportedCapability>([&]{makeReachability(d,ReachabilityMethod::BoundedDerivative,{true});});
}
std::shared_ptr<const ITransitionSystem> thermostatGraph() {
    auto f=fixtures::thermostat();
    auto r=makeReachability(makeThermostatDynamics(),ReachabilityMethod::CooperativeMonotone);
    return buildSymbolicModel(*f.grid,*f.inputs,*r,{{10},{15}});
}
void graph() {
    const auto g=thermostatGraph(); // All temporary dependencies already destroyed.
    CHECK(g->internalStateCount()==3 && g->inputCount()==2);
    for(const auto& row:test::thermostat_oracle){const auto& s=g->successors(row.state,row.input);
        CHECK(s.kind==SuccessorKind::Explicit && s.ids==row.ids);validateSuccessors(s,3);CHECK(nonempty(s,3));}
    for(InputId a=0;a<2;++a){const auto& s=g->successors(0,a);CHECK(s.kind==SuccessorKind::AllStates && s.ids.empty());CHECK(contains(s,0,3));}
    const auto* reference=&g->successors(1,0); (void)g->successors(3,1); CHECK(reference==&g->successors(1,0));
    test::throws<std::out_of_range>([&]{g->successors(4,0);}); test::throws<std::out_of_range>([&]{g->successors(1,2);});
    CHECK(g->statistics().processed_pairs==8); CHECK(g->statistics().stored_edges==12);
}
class FixedEnclosure final : public IReachability {
    Box box_;
public:
    explicit FixedEnclosure(Box box):box_(std::move(box)){}
    std::size_t stateDimension()const noexcept override{return 1;}
    std::size_t inputDimension()const noexcept override{return 1;}
    std::size_t disturbanceDimension()const noexcept override{return 1;}
    NumericGuarantee numericGuarantee()const noexcept override{return NumericGuarantee::UncertifiedFloat;}
    Box enclose(const Box&,const Vector&,const Box&)const override{return box_;}
};
void boundaries() {
    auto f=fixtures::thermostat();
    const std::vector<std::pair<Box,std::vector<StateId>>> cases{
        {{{16},{18}},{0,1}}, {{{15},{16}},{0}}, {{{17},{17}},{0,1}},
        {{{19},{19}},{1,2}}, {{{23},{23}},{0,3}}, {{{16},{24}},{0,1,2,3}}
    };
    for(const auto& c:cases){FixedEnclosure r(c.first);auto g=buildSymbolicModel(*f.grid,*f.inputs,r,{{10},{15}});
        CHECK(g->successors(1,0).kind==SuccessorKind::Explicit); CHECK(g->successors(1,0).ids==c.second);}
    FixedEnclosure bad({{20},{19}});
    test::throws<std::invalid_argument>([&]{buildSymbolicModel(*f.grid,*f.inputs,bad,{{10},{15}});});
}
void limits() {
    auto f=fixtures::thermostat();auto r=makeReachability(makeThermostatDynamics(),ReachabilityMethod::CooperativeMonotone);
    for(BuildOptions options : {BuildOptions{7,100,100000},BuildOptions{100,1,100000},BuildOptions{100,100,1}})
        test::throws<std::length_error>([&]{buildSymbolicModel(*f.grid,*f.inputs,*r,{{10},{15}},options);});
}
void building() {
    auto d=makeRingBuildingDynamics(3);
    CHECK(d->stateDimension()==3 && d->inputDimension()==3 && d->disturbanceDimension()==0);
    const auto v=d->step({17,18,19},{0,0.3,0.6},{});
    CHECK(v.size()==3 && test::near(v[0],17.54) && test::near(v[1],18.009) && test::near(v[2],18.424));
    const auto off=d->step({20,20,20},{0,0,0},{}); const auto on=d->step({20,20,20},{0.6,0.6,0.6},{});
    for(std::size_t i=0;i<3;++i){CHECK(test::near(off[i],19.055));CHECK(test::near(on[i],20.675));}
    CHECK(d->cooperativeOn({{17,17,17},{22,22,22}},{0.6,0.6,0.6},{{},{}}));
    const auto bounds=d->absoluteJacobianBounds({{17,17,17},{22,22,22}},{0.6,0.6,0.6},{{},{}});
    validateDerivativeBounds(bounds,3,0); CHECK(test::near(bounds.Dx[0][0],0.001)); CHECK(test::near(bounds.Dx[0][2],0.45));
    for(auto method:{ReachabilityMethod::CooperativeMonotone,ReachabilityMethod::BoundedDerivative}) {
        const auto reach=makeReachability(d,method);
        equalBox(reach->enclose({{20,20,20},{20.5,20.5,20.5}},{0,0,0},{{},{}}),
                 {{19.055,19.055,19.055},{19.5325,19.5325,19.5325}});
    }
    auto four=makeRingBuildingDynamics(4);const auto z=four->step({17,18,19,20},{0,0,0,0},{});
    const Vector expected{17.99,17.145,18.1,17.255};for(std::size_t i=0;i<4;++i)CHECK(test::near(z[i],expected[i]));
    test::throws<std::invalid_argument>([]{makeRingBuildingDynamics(2);});
    test::throws<std::invalid_argument>([&]{d->step({20,20,20},{0,0,0},{1});});
}
void exportPackage() {
    auto f=fixtures::thermostat();auto g=thermostatGraph();auto m=f.metadata;m.provenance=Provenance::GeneratedPart1;
    auto b=assembleModelBundle(f.grid,f.inputs,g,m);validateBundle(b);
    const auto directory=std::filesystem::current_path()/"test-model-export";
    exportModelPackage(b,directory);
    for(const auto& name:{"manifest.json","cells.csv","inputs.csv","transitions.csv"}) CHECK(std::filesystem::is_regular_file(directory/name));
    std::ifstream in(directory/"transitions.csv");std::ostringstream actual;actual<<in.rdbuf();
    const std::string expected="state_id,input_id,kind,successor_id\n0,0,all,\n0,1,all,\n1,0,explicit,0\n1,0,explicit,1\n1,1,explicit,1\n1,1,explicit,2\n2,0,explicit,1\n2,0,explicit,2\n2,1,explicit,2\n2,1,explicit,3\n3,0,explicit,2\n3,0,explicit,3\n3,1,explicit,0\n3,1,explicit,3\n";
    CHECK(actual.str()==expected);
}
int main(int argc,char** argv){return test::run(argc,argv,{{"thermostat",thermostat},{"affine",affine},{"capabilities",capabilities},{"graph",graph},{"boundaries",boundaries},{"limits",limits},{"building",building},{"export",exportPackage}},true);}
