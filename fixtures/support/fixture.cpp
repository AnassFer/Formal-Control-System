#include "fixture.hpp"
#include <array>
namespace symctrl::fixtures {
namespace {
struct Cell { StateId id; GridIndex index; Box bounds; };
class TableGrid final : public IStateGrid {
    const GridSpec spec_;
    const std::array<Cell,3> cells_;
public:
    TableGrid(GridSpec spec, std::array<Cell,3> cells) : spec_(std::move(spec)), cells_(std::move(cells)) {}
    std::size_t stateDimension() const noexcept override { return 1; }
    const GridSpec& spec() const noexcept override { return spec_; }
    StateId internalStateCount() const noexcept override { return 3; }
    StateId locate(const Vector& x) const override {
        validateVector(x, 1);
        // Lookup only the three explicit records, not an arithmetic grid algorithm.
        for (const auto& c : cells_) {
            if (x[0] >= c.bounds.lo[0] && (x[0] < c.bounds.hi[0]
                || (x[0] == c.bounds.hi[0] && x[0] == spec_.domain.hi[0]))) return c.id;
        }
        return kOutside;
    }
    Box cellBounds(StateId state) const override {
        for (const auto& c : cells_) if (c.id == state) return c.bounds;
        throw std::out_of_range("fixture has no finite OUT/invalid cell");
    }
    GridIndex indices(StateId state) const override {
        for (const auto& c : cells_) if (c.id == state) return c.index;
        throw std::out_of_range("fixture state ID");
    }
    StateId fromIndices(const GridIndex& index) const override {
        if (index.size() != 1) throw std::invalid_argument("fixture index shape");
        for (const auto& c : cells_) if (c.index == index) return c.id;
        throw std::out_of_range("fixture index");
    }
    std::vector<StateId> intersectingInternalCells(const Box& box) const override {
        validateBox(box);
        if (box.lo.size() != 1) throw std::invalid_argument("fixture box dimension");
        std::vector<StateId> result;
        for (const auto& c : cells_)
            if (box.lo[0] <= c.bounds.hi[0] && box.hi[0] >= c.bounds.lo[0]) result.push_back(c.id);
        return result;
    }
};
class TableInputs final : public IInputCatalog {
    const std::array<Vector,2> values_{{{0},{1}}};
public:
    std::size_t inputDimension() const noexcept override { return 1; }
    InputId inputCount() const noexcept override { return 2; }
    const Vector& value(InputId id) const override {
        if (id > 1) throw std::out_of_range("fixture input ID");
        return values_.at(checkedSize(id));
    }
};
class TableGraph final : public ITransitionSystem {
    const std::array<std::array<SuccessorSet,2>,4> rows_;
    const BuildStatistics statistics_{}; // No algorithm run; timings/processed counts are absent (zero).
public:
    explicit TableGraph(std::array<std::array<SuccessorSet,2>,4> rows) : rows_(std::move(rows)) {}
    StateId internalStateCount() const noexcept override { return 3; }
    InputId inputCount() const noexcept override { return 2; }
    NumericGuarantee numericGuarantee() const noexcept override { return NumericGuarantee::UncertifiedFloat; }
    const BuildStatistics& statistics() const noexcept override { return statistics_; }
    const SuccessorSet& successors(StateId state, InputId input) const override {
        if (state > 3 || input > 1) throw std::out_of_range("fixture graph ID");
        return rows_.at(checkedSize(state)).at(checkedSize(input));
    }
};
SuccessorSet explicitIds(std::initializer_list<StateId> ids) { return {SuccessorKind::Explicit, ids}; }
ModelMetadata metadata(const std::string& id, GridSpec spec, Box disturbance) {
    ModelMetadata m;
    m.model_id = id; m.source = "hand-authored Section 8/18 fixture; no producer algorithm executed";
    m.state_dimension = 1; m.input_dimension = 1; m.disturbance_dimension = disturbance.lo.size();
    m.internal_states = 3; m.input_count = 2; m.grid = std::move(spec); m.disturbance = std::move(disturbance);
    return m;
}
}
ModelBundle thermostat() {
    GridSpec spec{{{17},{23}},{3}};
    std::array<Cell,3> cells{{{1,{0},{{17},{19}}}, {2,{1},{{19},{21}}}, {3,{2},{{21},{23}}}}};
    const SuccessorSet all{SuccessorKind::AllStates,{}};
    std::array<std::array<SuccessorSet,2>,4> rows{{
        {{all,all}}, {{explicitIds({0,1}), explicitIds({1,2})}},
        {{explicitIds({1,2}), explicitIds({2,3})}}, {{explicitIds({2,3}), explicitIds({0,3})}}
    }};
    auto m = metadata("thermostat_fixture",spec,{{10},{15}});
    m.model_parameters = {{"a",0.1},{"b",0.2},{"T",30}};
    ModelBundle b{std::make_shared<TableGrid>(spec,cells),std::make_shared<TableInputs>(),
                  std::make_shared<TableGraph>(rows),m};
    validateBundle(b); return b;
}
ModelBundle nondeterministicChoice() {
    GridSpec spec{{{0},{3}},{3}};
    std::array<Cell,3> cells{{{1,{0},{{0},{1}}}, {2,{1},{{1},{2}}}, {3,{2},{{2},{3}}}}};
    const SuccessorSet all{SuccessorKind::AllStates,{}};
    std::array<std::array<SuccessorSet,2>,4> rows{{
        {{all,all}}, {{explicitIds({2,3}), explicitIds({2})}},
        {{explicitIds({2}), explicitIds({2})}}, {{explicitIds({3}), explicitIds({3})}}
    }};
    ModelBundle b{std::make_shared<TableGrid>(spec,cells),std::make_shared<TableInputs>(),
                  std::make_shared<TableGraph>(rows),metadata("nondeterministic_choice_fixture",spec,{{},{}})};
    validateBundle(b); return b;
}
}
