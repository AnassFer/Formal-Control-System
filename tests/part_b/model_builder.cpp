#include "test.hpp"
#include "symctrl/contracts/factories.hpp"
#include <limits>
#include <memory>
#include <utility>

using namespace symctrl;

namespace {
class TableGrid final : public IStateGrid {
public:
    GridSpec specification{{{0,0},{2,1}}, {2,1}};
    std::vector<Box> cells{{{0,0},{1,1}}, {{1,0},{2,1}}};
    std::size_t dimension = 2;
    StateId count = 2;
    mutable std::size_t bounds_calls = 0;
    std::function<std::vector<StateId>(const Box&)> intersections;

    std::size_t stateDimension() const noexcept override { return dimension; }
    const GridSpec& spec() const noexcept override { return specification; }
    StateId internalStateCount() const noexcept override { return count; }
    StateId locate(const Vector& point) const override {
        validateVector(point, dimension);
        for (std::size_t i = 0; i < cells.size(); ++i) {
            bool inside = true;
            for (std::size_t d = 0; d < dimension; ++d)
                inside = inside && point[d] >= cells[i].lo[d]
                    && (point[d] < cells[i].hi[d]
                        || (point[d] == cells[i].hi[d] && point[d] == specification.domain.hi[d]));
            if (inside) return i + 1;
        }
        return kOutside;
    }
    Box cellBounds(StateId state) const override {
        ++bounds_calls;
        if (state == kOutside || state > cells.size()) throw std::out_of_range("test cell ID");
        return cells.at(checkedSize(state - 1));
    }
    GridIndex indices(StateId state) const override {
        if (state < 1 || state > 2) throw std::out_of_range("test cell ID");
        return {static_cast<std::uint32_t>(state - 1), 0};
    }
    StateId fromIndices(const GridIndex& index) const override {
        if (index.size() != 2) throw std::invalid_argument("test index dimension");
        if (index[0] > 1 || index[1] != 0) throw std::out_of_range("test index");
        return index[0] + 1;
    }
    std::vector<StateId> intersectingInternalCells(const Box& box) const override {
        if (intersections) return intersections(box);
        std::vector<StateId> result;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            bool overlaps = true;
            for (std::size_t d = 0; d < dimension; ++d)
                overlaps = overlaps && box.lo[d] <= cells[i].hi[d] && box.hi[d] >= cells[i].lo[d];
            if (overlaps) result.push_back(i + 1);
        }
        return result;
    }
};

class TableInputs final : public IInputCatalog {
public:
    std::vector<Vector> values{{0,1}, {2,3}};
    std::size_t dimension = 2;
    InputId count = 2;
    std::size_t inputDimension() const noexcept override { return dimension; }
    InputId inputCount() const noexcept override { return count; }
    const Vector& value(InputId input) const override { return values.at(checkedSize(input)); }
};

class Enclosure final : public IReachability {
public:
    std::size_t n = 2, m = 2, q = 0;
    NumericGuarantee guarantee = NumericGuarantee::UncertifiedFloat;
    mutable std::size_t calls = 0;
    std::function<Box(const Box&, const Vector&, const Box&)> callback =
        [](const Box&, const Vector&, const Box&) { return Box{{0.5,0.5},{1.5,0.5}}; };
    std::size_t stateDimension() const noexcept override { return n; }
    std::size_t inputDimension() const noexcept override { return m; }
    std::size_t disturbanceDimension() const noexcept override { return q; }
    NumericGuarantee numericGuarantee() const noexcept override { return guarantee; }
    Box enclose(const Box& cell, const Vector& input, const Box& disturbance) const override {
        ++calls;
        return callback(cell, input, disturbance);
    }
};

void graph() {
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    reachability.callback = [](const Box& cell, const Vector& input, const Box& disturbance) {
        CHECK(cell.lo.size() == 2 && cell.hi.size() == 2);
        CHECK(disturbance.lo.empty() && disturbance.hi.empty());
        if (input == Vector({0,1})) return Box{{0.5,0.5},{1.5,0.5}};
        CHECK(input == Vector({2,3}));
        return Box{{3,0.5},{4,0.5}};
    };
    const auto result = buildSymbolicModel(grid, inputs, reachability, {{},{}});
    CHECK(result->internalStateCount() == 2 && result->inputCount() == 2);
    CHECK(result->numericGuarantee() == NumericGuarantee::UncertifiedFloat);
    for (StateId state : {StateId{1}, StateId{2}}) {
        CHECK(result->successors(state, 0).ids == std::vector<StateId>({1,2}));
        CHECK(result->successors(state, 1).ids == std::vector<StateId>({0}));
    }
    for (InputId input : {InputId{0}, InputId{1}}) {
        const auto& outside = result->successors(0, input);
        CHECK(outside.kind == SuccessorKind::AllStates && outside.ids.empty());
        CHECK(contains(outside, 0, 2) && contains(outside, 2, 2));
    }
    CHECK(grid.bounds_calls == 2 && reachability.calls == 4);
    CHECK(result->statistics().processed_pairs == 6);
    CHECK(result->statistics().stored_edges == 6);
    CHECK(std::isfinite(result->statistics().abstraction_seconds));
    CHECK(result->statistics().abstraction_seconds >= 0);
    test::throws<std::out_of_range>([&] { result->successors(3, 0); });
    test::throws<std::out_of_range>([&] { result->successors(0, 2); });
}

void boundaries() {
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    const std::vector<std::pair<Box,std::vector<StateId>>> cases{
        {{{1,0.5},{1,0.5}}, {1,2}},
        {{{0,0.5},{0,0.5}}, {0,1}},
        {{{2,0.5},{2,0.5}}, {0,2}},
        {{{3,0.5},{4,0.5}}, {0}},
        {{{-1,-1},{3,2}}, {0,1,2}}
    };
    for (const auto& example : cases) {
        reachability.callback = [box = example.first](const Box&, const Vector&, const Box&) { return box; };
        const auto result = buildSymbolicModel(grid, inputs, reachability, {{},{}});
        CHECK(result->successors(1, 0).kind == SuccessorKind::Explicit);
        CHECK(result->successors(1, 0).ids == example.second);
    }
}

void invalidViews() {
    const auto rejects = [](TableGrid grid, TableInputs inputs, Enclosure reachability) {
        test::throws<std::invalid_argument>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}); });
    };
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    grid.dimension = 3; rejects(grid, inputs, reachability); grid = TableGrid{};
    grid.count = 3; rejects(grid, inputs, reachability); grid = TableGrid{};
    grid.specification.cells_per_dimension = {0,1}; rejects(grid, inputs, reachability); grid = TableGrid{};
    inputs.dimension = 0; rejects(grid, inputs, reachability); inputs = TableInputs{};
    inputs.count = 0; rejects(grid, inputs, reachability); inputs = TableInputs{};
    inputs.values[0] = {0}; rejects(grid, inputs, reachability); inputs = TableInputs{};
    inputs.values[0][0] = std::numeric_limits<double>::infinity(); rejects(grid, inputs, reachability);
    inputs = TableInputs{};
    reachability.n = 1; rejects(grid, inputs, reachability); reachability = Enclosure{};
    reachability.m = 1; rejects(grid, inputs, reachability); reachability = Enclosure{};
    reachability.q = 1; rejects(grid, inputs, reachability); reachability = Enclosure{};
    reachability.guarantee = static_cast<NumericGuarantee>(99); rejects(grid, inputs, reachability);
    reachability = Enclosure{};
    const std::vector<Box> badCells{
        {{},{}} , {{0},{1}}, {{1,0},{0,1}}, {{-1,0},{1,1}}, {{0,0},{0,1}},
        {{0,0},{1,std::numeric_limits<double>::quiet_NaN()}}
    };
    for (const auto& cell : badCells) {
        grid.cells[0] = cell;
        rejects(grid, inputs, reachability);
    }
}

void invalidEnclosures() {
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    const std::vector<Box> invalid{
        {{},{}}, {{0},{1}}, {{0,0},{1}}, {{2,0},{1,1}},
        {{0,0},{1,std::numeric_limits<double>::infinity()}}
    };
    for (const auto& box : invalid) {
        reachability.callback = [box](const Box&, const Vector&, const Box&) { return box; };
        test::throws<std::invalid_argument>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}); });
    }
    reachability.callback = [](const Box&, const Vector&, const Box&) -> Box {
        throw std::overflow_error("test enclosure overflow");
    };
    test::throws<std::overflow_error>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}); });
}

void invalidHits() {
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    for (const std::vector<StateId>& hits : std::vector<std::vector<StateId>>{{}, {2,1}, {1,1}, {0,1}, {3}}) {
        grid.intersections = [hits](const Box&) { return hits; };
        test::throws<std::invalid_argument>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}); });
    }
}

void limits() {
    TableGrid grid;
    TableInputs inputs;
    Enclosure reachability;
    for (const auto options : {BuildOptions{5,100,100000}, BuildOptions{100,1,100000},
                               BuildOptions{100,100,1}}) {
        test::throws<std::length_error>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}, options); });
    }
    const auto max = std::numeric_limits<std::uint32_t>::max();
    grid.specification.cells_per_dimension = {max, max};
    grid.count = static_cast<StateId>(max)*max;
    inputs.count = 3;
    grid.bounds_calls = 0;
    const BuildOptions unlimited{std::numeric_limits<std::uint64_t>::max(),
        std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max()};
    test::throws<std::overflow_error>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}, unlimited); });
    inputs.count = 1;
    test::throws<std::length_error>([&] { buildSymbolicModel(grid, inputs, reachability, {{},{}}, unlimited); });
    CHECK(grid.bounds_calls == 0);
}

void lifetime() {
    const auto result = [] {
        TableGrid grid;
        TableInputs inputs;
        Enclosure reachability;
        return buildSymbolicModel(grid, inputs, reachability, {{},{}});
    }();
    const auto* first = &result->successors(1, 0);
    const auto& ids = first->ids;
    (void)result->successors(2, 1);
    (void)result->successors(0, 0);
    CHECK(first == &result->successors(1, 0));
    CHECK(ids == std::vector<StateId>({1,2}));
}

void bundle() {
    auto grid = std::make_shared<TableGrid>();
    auto inputs = std::make_shared<TableInputs>();
    Enclosure reachability;
    auto graphView = buildSymbolicModel(*grid, *inputs, reachability, {{},{}});
    ModelMetadata metadata;
    metadata.model_id = "generic_2d";
    metadata.source = "analytic graph-builder test";
    metadata.state_dimension = 2;
    metadata.input_dimension = 2;
    metadata.internal_states = 2;
    metadata.input_count = 2;
    metadata.grid = grid->spec();
    auto result = assembleModelBundle(grid, inputs, graphView, metadata);
    validateBundle(result);
    CHECK(result.grid.get() == grid.get() && result.inputs.get() == inputs.get());
    CHECK(result.transitions.get() == graphView.get());
    metadata.model_id = "caller changed";
    CHECK(result.metadata.model_id == "generic_2d");
    test::throws<std::invalid_argument>([&] { assembleModelBundle(nullptr, inputs, graphView, metadata); });
    metadata.internal_states = 3;
    test::throws<std::invalid_argument>([&] { assembleModelBundle(grid, inputs, graphView, metadata); });
    grid.reset(); inputs.reset(); graphView.reset();
    validateBundle(result);
    CHECK(result.transitions->successors(2, 1).ids == std::vector<StateId>({1,2}));
}
}

int main(int argc, char** argv) {
    return test::run(argc, argv, {{"graph", graph}, {"boundaries", boundaries},
        {"invalid_views", invalidViews}, {"invalid_enclosures", invalidEnclosures},
        {"invalid_hits", invalidHits}, {"limits", limits}, {"lifetime", lifetime}, {"bundle", bundle}});
}
