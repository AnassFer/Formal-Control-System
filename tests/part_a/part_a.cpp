#include "test.hpp"
#include "oracle.hpp"
#include "symctrl/contracts/factories.hpp"
#include <limits>
using namespace symctrl;
void indexing() {
    auto one=makeUniformGrid({{{-3},{3}},{3}});
    CHECK(one->fromIndices({2})==3); CHECK(one->indices(2)==GridIndex({1}));
    auto two=makeUniformGrid({{{-2,5},{2,11}},{2,3}});
    const std::vector<GridIndex> expected{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}};
    for(StateId id=1;id<=6;++id){CHECK(two->indices(id)==expected.at(checkedSize(id-1))); CHECK(two->fromIndices(two->indices(id))==id);}
    auto three=makeUniformGrid({{{2,-4,7},{4,0,13}},{2,2,3}});
    CHECK(three->fromIndices({1,0,1})==6); CHECK(three->indices(12)==GridIndex({1,1,2}));
    for(StateId id=1;id<=12;++id) CHECK(three->fromIndices(three->indices(id))==id);
}
void geometry() {
    auto g=makeUniformGrid({{{-3},{3}},{3}});
    CHECK(g->locate({-3})==1); CHECK(g->locate({-2})==1); CHECK(g->locate({-1})==2);
    CHECK(g->locate({1})==3); CHECK(g->locate({3})==3);
    CHECK(g->locate({std::nextafter(-3.0,-4.0)})==0); CHECK(g->locate({std::nextafter(3.0,4.0)})==0);
    CHECK(g->cellBounds(2).lo==Vector({-1})); CHECK(g->cellBounds(2).hi==Vector({1}));
    CHECK(g->intersectingInternalCells({{-1},{-1}})==std::vector<StateId>({1,2}));
    auto grid=makeUniformGrid({{{-2,5},{2,9}},{2,2}});
    CHECK(grid->locate({0,7})==4); CHECK(grid->locate({2,9})==4);
    for(const Box& b : std::vector<Box>{{{-1,6},{1,8}},{{0,7},{0,7}},{{-5,6},{-3,8}},{{2,9},{2,9}}}) {
        std::vector<StateId> brute;
        for(StateId id=1;id<=4;++id){const auto c=grid->cellBounds(id); bool hit=true;
            for(std::size_t d=0;d<2;++d) hit=hit && b.lo[d]<=c.hi[d] && b.hi[d]>=c.lo[d];
            if(hit)brute.push_back(id);
        }
        CHECK(grid->intersectingInternalCells(b)==brute);
    }
}
void inputs() {
    auto inputs=makeInputCatalog({{2,-1},{0,3},{2,-1}});
    CHECK(inputs->inputDimension()==2); CHECK(inputs->inputCount()==3);
    CHECK(inputs->value(0)==Vector({2,-1})); CHECK(inputs->value(1)==Vector({0,3}));
    auto cart=makeCartesianInputs({{2,4},{-1,3},{7}});
    CHECK(cart->inputCount()==4); CHECK(cart->value(0)==Vector({2,-1,7}));
    CHECK(cart->value(1)==Vector({4,-1,7})); CHECK(cart->value(2)==Vector({2,3,7}));
    CHECK(cart->value(3)==Vector({4,3,7}));
    test::throws<std::out_of_range>([&]{cart->value(4);});
    test::throws<std::invalid_argument>([]{makeInputCatalog({});});
    test::throws<std::invalid_argument>([]{makeInputCatalog({{}});});
    test::throws<std::invalid_argument>([]{makeInputCatalog({{1},{1,2}});});
    test::throws<std::invalid_argument>([]{makeCartesianInputs({{1},{}});});
    test::throws<std::invalid_argument>([]{makeCartesianInputs({{std::numeric_limits<double>::infinity()}});});
}
void invalid() {
    auto g=makeUniformGrid({{{0},{1}},{1}});
    test::throws<std::out_of_range>([&]{g->cellBounds(0);});
    test::throws<std::out_of_range>([&]{g->indices(2);});
    test::throws<std::out_of_range>([&]{g->fromIndices({1});});
    test::throws<std::invalid_argument>([&]{g->locate({});});
    test::throws<std::invalid_argument>([&]{g->locate({std::numeric_limits<double>::quiet_NaN()});});
    test::throws<std::invalid_argument>([]{makeUniformGrid({{{0},{1}},{0}});});
    test::throws<std::invalid_argument>([]{makeUniformGrid({{{0},{0}},{1}});});
    test::throws<std::invalid_argument>([]{makeUniformGrid({{{0},{1}},{1,2}});});
    test::throws<std::overflow_error>([]{makeUniformGrid({{{0,0,0},{1,1,1}},{UINT32_MAX,UINT32_MAX,UINT32_MAX}});});
}
class MemoryPolicy final : public ISymbolicPolicy {
public:
    PolicyDecision decide(PolicyMemory previous,StateId state) const override {
        if(state!=1)throw std::out_of_range("missing policy entry");
        const PolicyMemory next{checkedAdd(previous.value,1)};
        return {next,next.value%2};
    }
};
class InvalidPolicy final : public ISymbolicPolicy {
public: PolicyDecision decide(PolicyMemory m,StateId) const override {return {m,99};}
};
void policy() {
    auto g=makeUniformGrid({{{17},{23}},{3}}); auto inputs=makeInputCatalog({{0},{1}});
    MemoryPolicy p; const auto r=refineDecision(*g,*inputs,p,{18},{7});
    CHECK(r.state==1 && r.input==0 && r.next_memory.value==8 && r.physical_input==Vector({0}));
    test::SuppliedPolicy supplied; const auto on=refineDecision(*g,*inputs,supplied,{18});
    CHECK(on.input==1 && on.physical_input==Vector({1}));
    test::throws<std::out_of_range>([&]{refineDecision(*g,*inputs,p,{20});});
    test::throws<std::out_of_range>([&]{refineDecision(*g,*inputs,p,{24});});
    InvalidPolicy bad; test::throws<std::out_of_range>([&]{refineDecision(*g,*inputs,bad,{18});});
}
int main(int argc,char** argv){return test::run(argc,argv,{{"indexing",indexing},{"geometry",geometry},{"inputs",inputs},{"invalid",invalid},{"policy",policy}},true);}
