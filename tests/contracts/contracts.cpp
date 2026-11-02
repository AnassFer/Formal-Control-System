#include "test.hpp"
#include "symctrl/contracts/factories.hpp"
#include <limits>
#include <type_traits>
using namespace symctrl;
void shapes() {
    validateBox({{1},{1}});
    validateBox({{},{}},true);
    test::throws<std::invalid_argument>([]{validateBox({{}, {}});});
    test::throws<std::invalid_argument>([]{validateBox({{1},{0}});});
    test::throws<std::invalid_argument>([]{validateBox({{1},{1,2}});});
    test::throws<std::invalid_argument>([]{validateVector({std::numeric_limits<double>::infinity()},1);});
    test::throws<std::invalid_argument>([]{validateVector({std::numeric_limits<double>::quiet_NaN()},1);});
    validateDerivativeBounds({{{1,2},{3,4}},{{},{}}},2,0);
    test::throws<std::invalid_argument>([]{validateDerivativeBounds({{{1}},{{-1}}},1,1);});
    test::throws<std::invalid_argument>([]{validateDerivativeBounds({{{1,2}},{{0}}},1,1);});
    test::throws<std::invalid_argument>([]{validateDerivativeBounds({{{1}},{}},1,0);});
}
void counts() {
    CHECK(validateGridSpec({{{-1,2},{1,6}},{2,4}}) == 8);
    test::throws<std::invalid_argument>([]{validateGridSpec({{{0},{1}},{0}});});
    test::throws<std::invalid_argument>([]{validateGridSpec({{{0},{0}},{1}});});
    test::throws<std::invalid_argument>([]{validateGridSpec({{{0},{1}},{1,2}});});
    test::throws<std::overflow_error>([]{checkedAdd(UINT64_MAX,1);});
    test::throws<std::overflow_error>([]{checkedMultiply(UINT64_MAX,2);});
    test::throws<std::overflow_error>([]{validateGridSpec({{{0,0,0},{1,1,1}},{UINT32_MAX,UINT32_MAX,UINT32_MAX}});});
    CHECK(checkedMultiply(0,UINT64_MAX)==0); CHECK(checkedSize(7)==7);
}
void successors() {
    SuccessorSet all{SuccessorKind::AllStates,{}};
    SuccessorSet empty{SuccessorKind::Explicit,{}};
    CHECK(nonempty(all,3)); CHECK(!nonempty(empty,3));
    CHECK(contains(all,0,3)); CHECK(contains(all,3,3));
    std::vector<StateId> visited;
    forEachSuccessor(all,3,[&](StateId id){visited.push_back(id);});
    CHECK(visited == std::vector<StateId>({0,1,2,3})); CHECK(all.ids.empty());
    visited.clear(); forEachSuccessor(all,0,[&](StateId id){visited.push_back(id);});
    CHECK(visited == std::vector<StateId>({0}));
    CHECK(contains({SuccessorKind::Explicit,{0,2}},2,3));
    CHECK(!contains({SuccessorKind::Explicit,{0,2}},1,3));
    test::throws<std::invalid_argument>([]{validateSuccessors({SuccessorKind::Explicit,{2,1}},3);});
    test::throws<std::invalid_argument>([]{validateSuccessors({SuccessorKind::Explicit,{1,1}},3);});
    test::throws<std::invalid_argument>([]{validateSuccessors({SuccessorKind::Explicit,{4}},3);});
    test::throws<std::invalid_argument>([]{validateSuccessors({SuccessorKind::AllStates,{0}},3);});
    test::throws<std::out_of_range>([&]{contains(all,4,3);});
    // Overflow-safe iteration even for M == UINT64_MAX: visitor stops via an exception.
    test::throws<std::out_of_range>([&]{forEachSuccessor(all,UINT64_MAX,[](StateId id){if(id==2) throw std::out_of_range("stop");});});
}
void metadata() {
    static_assert(std::is_abstract<IStateGrid>::value && std::is_abstract<IDynamics>::value,"pure contracts");
    static_assert(std::is_abstract<IInputCatalog>::value && std::is_abstract<IReachability>::value,"pure contracts");
    CHECK(std::string(toString(NumericGuarantee::UncertifiedFloat))=="uncertified_float");
    CHECK(ModelMetadata{}.numeric_guarantee == NumericGuarantee::UncertifiedFloat);
    test::throws<std::invalid_argument>([]{validateBundle({});});
}
int main(int argc,char** argv){return test::run(argc,argv,{{"shapes",shapes},{"counts",counts},{"successors",successors},{"metadata",metadata}});}
