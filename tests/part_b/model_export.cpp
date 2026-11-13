#include "test.hpp"
#include "fixture.hpp"
#include "symctrl/contracts/factories.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <utility>

using namespace symctrl;
namespace fs = std::filesystem;

namespace {
class TemporaryDirectory {
public:
    fs::path path;
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("symctrl-export-test-" + std::to_string(stamp));
        if (!fs::create_directory(path)) throw std::runtime_error("test temporary directory collision");
    }
    ~TemporaryDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
};

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    CHECK(in.good());
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

class TinyGrid final : public IStateGrid {
public:
    GridSpec definition{{{0,10},{2,12}}, {2,1}};
    std::vector<Box> cells{{{0,10},{1,12}}, {{1,10},{2,12}}};
    bool badIndex = false;
    std::size_t stateDimension() const noexcept override { return 2; }
    const GridSpec& spec() const noexcept override { return definition; }
    StateId internalStateCount() const noexcept override { return 2; }
    StateId locate(const Vector&) const override { throw std::runtime_error("export does not locate points"); }
    Box cellBounds(StateId id) const override {
        if (id == 0 || id > 2) throw std::out_of_range("test grid cell");
        return cells.at(static_cast<std::size_t>(id - 1));
    }
    GridIndex indices(StateId id) const override {
        if (id == 0 || id > 2) throw std::out_of_range("test grid index");
        return {badIndex ? 0u : static_cast<std::uint32_t>(id - 1), 0};
    }
    StateId fromIndices(const GridIndex& index) const override {
        if (index.size() != 2 || index[0] > 1 || index[1] != 0) throw std::out_of_range("test grid indices");
        return 1 + index[0];
    }
    std::vector<StateId> intersectingInternalCells(const Box&) const override {
        throw std::runtime_error("export does not construct transitions");
    }
};

class TinyInputs final : public IInputCatalog {
public:
    std::vector<Vector> values{{0.1, std::nextafter(1.0, 2.0)}, {-0.0, -2.5}};
    std::size_t inputDimension() const noexcept override { return 2; }
    InputId inputCount() const noexcept override { return 2; }
    const Vector& value(InputId input) const override { return values.at(checkedSize(input)); }
};

class TinyGraph final : public ITransitionSystem {
public:
    std::vector<SuccessorSet> rows{
        {SuccessorKind::AllStates,{}}, {SuccessorKind::AllStates,{}},
        {SuccessorKind::Explicit,{1,2}}, {SuccessorKind::Explicit,{0,2}},
        {SuccessorKind::AllStates,{}}, {SuccessorKind::Explicit,{2}}
    };
    BuildStatistics stats{};
    StateId internalStateCount() const noexcept override { return 2; }
    InputId inputCount() const noexcept override { return 2; }
    NumericGuarantee numericGuarantee() const noexcept override { return NumericGuarantee::UncertifiedFloat; }
    const BuildStatistics& statistics() const noexcept override { return stats; }
    const SuccessorSet& successors(StateId state, InputId input) const override {
        if (state > 2 || input > 1) throw std::out_of_range("test graph query");
        return rows.at(checkedSize(state*2 + input));
    }
};

ModelBundle genericBundle() {
    auto grid = std::make_shared<TinyGrid>();
    ModelMetadata m;
    m.model_id = "generic_export";
    m.source = "independent two-dimensional export test";
    m.provenance = Provenance::GeneratedPart1;
    m.state_dimension = m.input_dimension = 2;
    m.disturbance_dimension = 0;
    m.internal_states = m.input_count = 2;
    m.grid = grid->definition;
    m.disturbance = {{},{}};
    return {grid, std::make_shared<TinyInputs>(), std::make_shared<TinyGraph>(), m};
}

void fixture() {
    TemporaryDirectory tmp;
    const auto directory = tmp.path / "model";
    exportModelPackage(fixtures::thermostat(), directory);
    CHECK(read(directory / "cells.csv") == "state_id,lower_0,upper_0\n1,17,19\n2,19,21\n3,21,23\n");
    CHECK(read(directory / "inputs.csv") == "input_id,u_0\n0,0\n1,1\n");
    CHECK(read(directory / "transitions.csv") ==
        "state_id,input_id,kind,successor_id\n0,0,all,\n0,1,all,\n"
        "1,0,explicit,0\n1,0,explicit,1\n1,1,explicit,1\n1,1,explicit,2\n"
        "2,0,explicit,1\n2,0,explicit,2\n2,1,explicit,2\n2,1,explicit,3\n"
        "3,0,explicit,2\n3,0,explicit,3\n3,1,explicit,0\n3,1,explicit,3\n");
    const auto manifest = read(directory / "manifest.json");
    CHECK(manifest.back() == '\n');
    CHECK(manifest.find("\"model_id\": \"thermostat_fixture\"") != std::string::npos);
    CHECK(manifest.find("\"provenance\": \"hand_authored_fixture\"") != std::string::npos);
    CHECK(manifest.find("\"numeric_guarantee\": \"uncertified_float\"") != std::string::npos);
}

void generic() {
    TemporaryDirectory tmp;
    auto bundle = genericBundle();
    exportModelPackage(bundle, tmp.path / "model");
    CHECK(read(tmp.path / "model/cells.csv") ==
        "state_id,lower_0,lower_1,upper_0,upper_1\n1,0,10,1,12\n2,1,10,2,12\n");
    CHECK(read(tmp.path / "model/transitions.csv") ==
        "state_id,input_id,kind,successor_id\n0,0,all,\n0,1,all,\n"
        "1,0,explicit,1\n1,0,explicit,2\n1,1,explicit,0\n1,1,explicit,2\n2,0,all,\n2,1,explicit,2\n");
    const auto manifest = read(tmp.path / "model/manifest.json");
    CHECK(manifest.find("\"dimensions\": {\"state\":2,\"input\":2,\"disturbance\":0}") != std::string::npos);
    CHECK(manifest.find("\"disturbance\": {\"lo\":[],\"hi\":[]}") != std::string::npos);
    CHECK(manifest.find("\"provenance\": \"generated_part1\"") != std::string::npos);
    CHECK(manifest.find("\"input_order\":\"explicit_caller_order\"") != std::string::npos);
    auto roundedGrid = std::make_shared<TinyGrid>();
    roundedGrid->cells[0].hi[0] = std::nextafter(1.0,2.0);
    roundedGrid->cells[1].lo[0] = std::nextafter(1.0,2.0);
    bundle.grid = roundedGrid;
    exportModelPackage(bundle, tmp.path / "rounded");
    CHECK(read(tmp.path / "rounded/cells.csv").find("1.0000000000000002") != std::string::npos);
}

void stringsAndPrecision() {
    class CommaPunctuation final : public std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    struct RestoreLocale {
        std::locale previous = std::locale();
        ~RestoreLocale() { std::locale::global(previous); }
    } restoreLocale;
    std::locale::global(std::locale(std::locale::classic(), new CommaPunctuation));
    TemporaryDirectory tmp;
    auto bundle = genericBundle();
    bundle.metadata.model_id = "quote\"slash\\\nmodel";
    bundle.metadata.source = "UTF-8 \xc3\xa9\t\x01";
    bundle.metadata.model_parameters = {{"key\"\\\n", std::nextafter(1.0,2.0)}};
    exportModelPackage(bundle, tmp.path / "model");
    const auto manifest = read(tmp.path / "model/manifest.json");
    CHECK(manifest.find("quote\\\"slash\\\\\\u000amodel") != std::string::npos);
    CHECK(manifest.find("UTF-8 \xc3\xa9\\u0009\\u0001") != std::string::npos);
    CHECK(manifest.find("key\\\"\\\\\\u000a") != std::string::npos);
    CHECK(manifest.find("1.0000000000000002") != std::string::npos);
    const auto inputText = read(tmp.path / "model/inputs.csv");
    std::istringstream input(inputText);
    std::string row;
    std::getline(input, row);
    std::getline(input, row);
    const auto firstComma = row.find(',');
    const auto secondComma = row.find(',', firstComma + 1);
    CHECK(std::stod(row.substr(firstComma + 1, secondComma - firstComma - 1)) == 0.1);
    CHECK(std::stod(row.substr(secondComma + 1)) == std::nextafter(1.0,2.0));
    CHECK(inputText.find('\r') == std::string::npos);
    CHECK(inputText.back() == '\n');
}

void limits() {
    TemporaryDirectory tmp;
    const auto target = tmp.path / "model";
    test::throws<std::length_error>([&] { exportModelPackage(fixtures::thermostat(), target, {13}); });
    CHECK(!fs::exists(target));
    test::throws<std::length_error>([&] { exportModelPackage(fixtures::thermostat(), target, {0}); });
    CHECK(!fs::exists(target));
    exportModelPackage(fixtures::thermostat(), target, {14});
    const auto before = read(target / "manifest.json");
    test::throws<std::length_error>([&] { exportModelPackage(genericBundle(), target, {7}); });
    CHECK(read(target / "manifest.json") == before);
    exportModelPackage(genericBundle(), target, {8}); // Universal rows stay one row each.
}

void invalidViews() {
    TemporaryDirectory tmp;
    const auto target = tmp.path / "model";
    const std::vector<SuccessorSet> badRows{
        {SuccessorKind::Explicit,{}}, {SuccessorKind::Explicit,{2,1}},
        {SuccessorKind::Explicit,{1,1}}, {SuccessorKind::Explicit,{3}},
        {SuccessorKind::AllStates,{1}}, {static_cast<SuccessorKind>(99),{}}
    };
    for (const auto& row : badRows) {
        auto bundle = genericBundle();
        auto graph = std::make_shared<TinyGraph>();
        graph->rows[2] = row;
        bundle.transitions = graph;
        test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
        CHECK(!fs::exists(target));
    }
    auto bundle = genericBundle();
    auto graph = std::make_shared<TinyGraph>();
    graph->rows[0] = {SuccessorKind::Explicit,{1}};
    bundle.transitions = graph;
    test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });

    for (const auto& bounds : std::vector<Box>{ {{0},{1}}, {{0,10},{0,12}}, {{-1,10},{1,12}}, {{0,10},{1.25,12}},
        {{0,10},{std::numeric_limits<double>::quiet_NaN(),12}} }) {
        bundle = genericBundle();
        auto grid = std::make_shared<TinyGrid>();
        grid->cells[0] = bounds;
        bundle.grid = grid;
        test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
    }
    bundle = genericBundle();
    auto grid = std::make_shared<TinyGrid>();
    grid->badIndex = true;
    bundle.grid = grid;
    test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
    grid->badIndex = false;
    grid->cells[0].hi[0] = grid->cells[1].lo[0] = 1.25; // Contiguous, but not the declared uniform grid.
    test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });

    for (const auto& value : std::vector<Vector>{{0}, {0,std::numeric_limits<double>::infinity()}}) {
        bundle = genericBundle();
        auto inputs = std::make_shared<TinyInputs>();
        inputs->values[0] = value;
        bundle.inputs = inputs;
        test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
    }
    bundle = genericBundle();
    bundle.metadata.input_count = 3;
    test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
    CHECK(!fs::exists(target));
}

void invalidStrings() {
    TemporaryDirectory tmp;
    const auto target = tmp.path / "model";
    for (const auto& text : std::vector<std::string>{"\xc3(", "\x80", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
        auto bundle = genericBundle();
        bundle.metadata.source = text;
        test::throws<std::invalid_argument>([&] { exportModelPackage(bundle, target); });
        CHECK(!fs::exists(target));
    }
}

void ioAndReplacement() {
    TemporaryDirectory tmp;
    const auto target = tmp.path / "model";
    test::throws<std::invalid_argument>([&] { exportModelPackage(genericBundle(), {}); });
    const auto fileTarget = tmp.path / "file";
    { std::ofstream out(fileTarget); out << "preserve"; }
    test::throws<fs::filesystem_error>([&] { exportModelPackage(genericBundle(), fileTarget); });
    CHECK(read(fileTarget) == "preserve");
    fs::create_directories(target / "cells.csv");
    test::throws<fs::filesystem_error>([&] { exportModelPackage(genericBundle(), target); });
    CHECK(fs::is_directory(target / "cells.csv"));
    fs::remove(target / "cells.csv");
    fs::create_symlink(fileTarget, target / "cells.csv");
    test::throws<fs::filesystem_error>([&] { exportModelPackage(genericBundle(), target); });
    CHECK(read(fileTarget) == "preserve");
    fs::remove(target / "cells.csv");
    { std::ofstream out(target / "notes.txt"); out << "preserve"; }
    exportModelPackage(fixtures::thermostat(), target);
    exportModelPackage(genericBundle(), target);
    CHECK(read(target / "notes.txt") == "preserve");
    CHECK(read(target / "manifest.json").find("generic_export") != std::string::npos);
    for (const auto& entry : fs::directory_iterator(target))
        CHECK(entry.path().filename().string().find(".symctrl-export-") != 0);
}
}

int main(int argc, char** argv) {
    return test::run(argc, argv, {{"fixture",fixture}, {"generic",generic},
        {"strings_precision",stringsAndPrecision}, {"limits",limits}, {"invalid_views",invalidViews},
        {"invalid_strings",invalidStrings}, {"io_replacement",ioAndReplacement}});
}
