#include "symctrl/factories.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <system_error>

namespace symctrl {
namespace {
namespace fs = std::filesystem;
constexpr std::array<const char*,4> filenames{{"manifest.json", "cells.csv", "inputs.csv", "transitions.csv"}};

void validateUtf8(const std::string& value) {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto first = static_cast<unsigned char>(value[i++]);
        if (first < 0x80) continue;
        std::size_t remaining;
        std::uint32_t codepoint;
        std::uint32_t minimum;
        if (first >= 0xc2 && first <= 0xdf) { remaining = 1; codepoint = first & 0x1fu; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { remaining = 2; codepoint = first & 0x0fu; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; codepoint = first & 0x07u; minimum = 0x10000; }
        else throw std::invalid_argument("metadata is not valid UTF-8");
        if (remaining > value.size() - i) throw std::invalid_argument("truncated UTF-8 metadata");
        for (std::size_t j = 0; j < remaining; ++j) {
            const auto next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xc0u) != 0x80u) throw std::invalid_argument("invalid UTF-8 continuation");
            codepoint = (codepoint << 6u) | (next & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
            throw std::invalid_argument("invalid UTF-8 codepoint");
    }
}

void quoted(std::ostream& out, const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (char raw : value) {
        const auto c = static_cast<unsigned char>(raw);
        if (c == '"' || c == '\\') out << '\\' << raw;
        else if (c < 0x20) out << "\\u00" << hex[c >> 4u] << hex[c & 0x0fu];
        else out << raw;
    }
    out << '"';
}

template<class Values> void array(std::ostream& out, const Values& values) {
    out << '[';
    bool first = true;
    for (const auto value : values) {
        if (!first) out << ',';
        out << value;
        first = false;
    }
    out << ']';
}

void box(std::ostream& out, const Box& value) {
    out << "{\"lo\":"; array(out, value.lo);
    out << ",\"hi\":"; array(out, value.hi); out << '}';
}

void manifest(std::ostream& out, const ModelMetadata& m) {
    out << "{\n  \"schema_version\": "; quoted(out, m.schema_version);
    out << ",\n  \"contract_version\": "; quoted(out, m.contract_version);
    out << ",\n  \"model_id\": "; quoted(out, m.model_id);
    out << ",\n  \"model_parameters\": {";
    bool first = true;
    for (const auto& entry : m.model_parameters) {
        if (!first) out << ',';
        quoted(out, entry.first); out << ':' << entry.second;
        first = false;
    }
    out << "},\n  \"provenance\": "; quoted(out, toString(m.provenance));
    out << ",\n  \"source\": "; quoted(out, m.source);
    out << ",\n  \"dimensions\": {\"state\":" << m.state_dimension
        << ",\"input\":" << m.input_dimension << ",\"disturbance\":" << m.disturbance_dimension << '}';
    out << ",\n  \"internal_states\": " << m.internal_states
        << ",\n  \"input_count\": " << m.input_count
        << ",\n  \"outside_state_id\": " << m.outside_state_id;
    out << ",\n  \"grid\": {\"domain\":"; box(out, m.grid.domain);
    out << ",\"cells_per_dimension\":"; array(out, m.grid.cells_per_dimension); out << '}';
    // The v1 catalogue exposes only its returned enumeration, not how it was constructed.
    out << ",\n  \"semantics\": {\"state_order\":\"dimension_0_fastest\","
           "\"input_order\":\"explicit_caller_order\",\"point_ownership\":\"half_open_last_closed\","
           "\"cell_geometry\":\"closed\",\"successor_intersection\":\"closure\","
           "\"outside_successors\":\"all_states\",\"enabled_inputs\":\"all_states_all_inputs\"}";
    out << ",\n  \"numeric_guarantee\": "; quoted(out, toString(m.numeric_guarantee));
    out << ",\n  \"disturbance\": "; box(out, m.disturbance);
    out << ",\n  \"files\": {\"cells\":\"cells.csv\",\"inputs\":\"inputs.csv\","
           "\"transitions\":\"transitions.csv\"}\n}\n";
}

void validateCell(const IStateGrid& grid, StateId id, const ModelMetadata& m) {
    const auto cell = grid.cellBounds(id);
    validateBox(cell);
    validateVector(cell.lo, m.state_dimension);
    const auto index = grid.indices(id);
    if (index.size() != m.state_dimension || grid.fromIndices(index) != id)
        throw std::invalid_argument("export grid index does not round trip");
    StateId offset = id - 1;
    for (std::size_t d = 0; d < m.state_dimension; ++d) {
        const auto count = m.grid.cells_per_dimension[d];
        if (index[d] != offset % count || cell.lo[d] >= cell.hi[d]
            || cell.lo[d] < m.grid.domain.lo[d] || cell.hi[d] > m.grid.domain.hi[d]
            || (index[d] == 0 && cell.lo[d] != m.grid.domain.lo[d])
            || (index[d] == count - 1 && cell.hi[d] != m.grid.domain.hi[d]))
            throw std::invalid_argument("invalid export cell geometry/index");
        const double lo = m.grid.domain.lo[d], hi = m.grid.domain.hi[d];
        const double scale = std::max({std::abs(lo), std::abs(hi), hi - lo});
        const double ulp = scale == std::numeric_limits<double>::max()
            ? scale - std::nextafter(scale, 0.0)
            : std::nextafter(scale, std::numeric_limits<double>::infinity()) - scale;
        // Check the ideal uniform boundary while allowing rounding in subtraction, division,
        // multiplication and addition at the domain's scale. This changes neither the returned
        // geometry nor point ownership, and avoids requiring one A-specific evaluation order.
        const auto matchesBoundary = [&](double actual, std::uint64_t boundary) {
            const auto ideal = static_cast<long double>(lo)
                + (static_cast<long double>(hi) - static_cast<long double>(lo))
                    * (static_cast<long double>(boundary) / count);
            return std::abs(static_cast<long double>(actual) - ideal) <= 4*static_cast<long double>(ulp);
        };
        if (!matchesBoundary(cell.lo[d], index[d])
            || !matchesBoundary(cell.hi[d], static_cast<std::uint64_t>(index[d]) + 1))
            throw std::invalid_argument("export cell bounds disagree with uniform grid metadata");
        offset /= count;
    }
}

void preflight(const ModelBundle& bundle, ExportOptions options) {
    validateBundle(bundle);
    const auto& m = bundle.metadata;
    validateUtf8(m.model_id);
    validateUtf8(m.source);
    for (const auto& entry : m.model_parameters) validateUtf8(entry.first);
    const auto pairs = checkedMultiply(checkedAdd(m.internal_states, 1), m.input_count);
    if (pairs > options.max_transition_rows) throw std::length_error("export transition row budget exceeded");
    for (StateId s = 1; s <= m.internal_states; ++s) validateCell(*bundle.grid, s, m);
    for (InputId a = 0; a < m.input_count; ++a) validateVector(bundle.inputs->value(a), m.input_dimension);
    std::uint64_t rows = 0;
    for (StateId s = 0; s <= m.internal_states; ++s) {
        for (InputId a = 0; a < m.input_count; ++a) {
            const auto& successors = bundle.transitions->successors(s, a);
            validateSuccessors(successors, m.internal_states);
            if ((successors.kind == SuccessorKind::Explicit && successors.ids.empty())
                || (s == kOutside && successors.kind != SuccessorKind::AllStates))
                throw std::invalid_argument("incomplete export graph or nonuniversal OUT");
            rows = checkedAdd(rows, successors.kind == SuccessorKind::AllStates
                ? 1 : static_cast<std::uint64_t>(successors.ids.size()));
            if (rows > options.max_transition_rows) throw std::length_error("export transition row budget exceeded");
        }
    }
}

template<class Writer> void writeFile(const fs::path& path, Writer writer) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    writer(out);
    out.close(); // Report deferred write/flush failures before publishing the package.
}

class StagingDirectory {
public:
    fs::path path;
    bool preserve = false;

    explicit StagingDirectory(const fs::path& parent) {
        static std::atomic<std::uint64_t> sequence{0};
        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = parent / (".symctrl-export-" + std::to_string(stamp) + '-' + std::to_string(sequence++));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("cannot create unique export staging directory");
    }
    ~StagingDirectory() {
        if (!preserve) { std::error_code ignored; fs::remove_all(path, ignored); }
    }
};

void publish(StagingDirectory& staging, const fs::path& directory) {
    std::array<bool,4> backedUp{}, installed{};
    try {
        // Retire the previous manifest first; publish the new one after all three CSV files.
        for (std::size_t i = 0; i < filenames.size(); ++i) {
            const auto target = directory / filenames[i];
            if (fs::exists(target)) {
                fs::rename(target, staging.path / (std::string("old-") + filenames[i]));
                backedUp[i] = true;
            }
        }
        for (std::size_t i : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{0}}) {
            fs::rename(staging.path / filenames[i], directory / filenames[i]);
            installed[i] = true;
        }
    } catch (...) {
        bool restored = true;
        for (std::size_t i = 0; i < filenames.size(); ++i) {
            if (installed[i]) { std::error_code error; fs::remove(directory / filenames[i], error); restored &= !error; }
        }
        for (std::size_t i = 1; i < filenames.size(); ++i) {
            if (backedUp[i]) {
                std::error_code error;
                fs::rename(staging.path / (std::string("old-") + filenames[i]), directory / filenames[i], error);
                restored &= !error;
            }
        }
        if (restored && backedUp[0]) {
            std::error_code error;
            fs::rename(staging.path / "old-manifest.json", directory / "manifest.json", error);
            restored &= !error;
        }
        if (!restored) {
            staging.preserve = true;
            std::throw_with_nested(std::runtime_error("export rollback failed; backups retained in " + staging.path.string()));
        }
        throw;
    }
}
}

void exportModelPackage(const ModelBundle& bundle, const std::filesystem::path& directory, ExportOptions options) {
    if (directory.empty()) throw std::invalid_argument("empty export directory");
    preflight(bundle, options); // Model and row-budget failures do not touch the destination.
    fs::create_directories(directory);
    for (const auto* name : filenames) {
        const auto target = directory / name;
        const auto status = fs::symlink_status(target);
        if (fs::exists(status) && !fs::is_regular_file(status))
            throw fs::filesystem_error("export destination is not a regular file", target,
                                       std::make_error_code(std::errc::file_exists));
    }
    StagingDirectory staging(directory);
    const auto& m = bundle.metadata;
    writeFile(staging.path / "manifest.json", [&](std::ostream& out) { manifest(out, m); });
    writeFile(staging.path / "cells.csv", [&](std::ostream& out) {
        out << "state_id";
        for (std::size_t d = 0; d < m.state_dimension; ++d) out << ",lower_" << d;
        for (std::size_t d = 0; d < m.state_dimension; ++d) out << ",upper_" << d;
        out << '\n';
        for (StateId s = 1; s <= m.internal_states; ++s) {
            const auto cell = bundle.grid->cellBounds(s);
            out << s;
            for (double x : cell.lo) out << ',' << x;
            for (double x : cell.hi) out << ',' << x;
            out << '\n';
        }
    });
    writeFile(staging.path / "inputs.csv", [&](std::ostream& out) {
        out << "input_id";
        for (std::size_t d = 0; d < m.input_dimension; ++d) out << ",u_" << d;
        out << '\n';
        for (InputId a = 0; a < m.input_count; ++a) {
            out << a;
            for (double u : bundle.inputs->value(a)) out << ',' << u;
            out << '\n';
        }
    });
    writeFile(staging.path / "transitions.csv", [&](std::ostream& out) {
        out << "state_id,input_id,kind,successor_id\n";
        for (StateId s = 0; s <= m.internal_states; ++s) {
            for (InputId a = 0; a < m.input_count; ++a) {
                const auto& successors = bundle.transitions->successors(s, a);
                if (successors.kind == SuccessorKind::AllStates) out << s << ',' << a << ",all,\n";
                else for (StateId next : successors.ids) out << s << ',' << a << ",explicit," << next << '\n';
            }
        }
    });
    publish(staging, directory);
}
}
