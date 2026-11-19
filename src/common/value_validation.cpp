#include "symctrl/dynamics.hpp"
#include <cmath>
#include <limits>

namespace symctrl {
// Input: a, b: unsigned 64-bit counts.
// Output: Their sum a + b as an unsigned 64-bit count.
// Throws: std::overflow_error if the sum cannot be represented.
std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) throw std::overflow_error("ID/count addition overflow");
    return a + b;
}
// Input: a, b: unsigned 64-bit counts.
// Output: Their product a * b as an unsigned 64-bit count.
// Throws: std::overflow_error if the product cannot be represented.
std::uint64_t checkedMultiply(std::uint64_t a, std::uint64_t b) {
    if (b != 0 && a > std::numeric_limits<std::uint64_t>::max() / b) throw std::overflow_error("ID/count product overflow");
    return a * b;
}
// Input: value: unsigned 64-bit count.
// Output: The same count represented as std::size_t.
// Throws: std::overflow_error if value does not fit std::size_t.
std::size_t checkedSize(std::uint64_t value) {
    if (value > std::numeric_limits<std::size_t>::max()) throw std::overflow_error("container size overflow");
    return static_cast<std::size_t>(value);
}
// Input: value: coordinates; dimension: required number of coordinates.
// Output: No return value; leaves value unchanged when its size and all coordinates are valid.
// Throws: std::invalid_argument for a dimension mismatch or NaN/infinite coordinate.
void validateVector(const Vector& value, std::size_t dimension) {
    if (value.size() != dimension) throw std::invalid_argument("vector dimension mismatch");
    for (double x : value) if (!std::isfinite(x)) throw std::invalid_argument("nonfinite coordinate");
}
// Input: box: lower/upper coordinate bounds; allow_empty_disturbance: whether zero dimensions are allowed.
// Output: No return value; confirms matching finite bounds with lo[d] <= hi[d], without modifying box.
// Throws: std::invalid_argument for invalid bounds or a disallowed empty box.
void validateBox(const Box& box, bool allow_empty_disturbance) {
    validateVector(box.lo, box.hi.size());
    validateVector(box.hi, box.lo.size());
    if (box.lo.empty() && !allow_empty_disturbance) throw std::invalid_argument("empty state box");
    for (std::size_t d = 0; d < box.lo.size(); ++d)
        if (box.lo[d] > box.hi[d]) throw std::invalid_argument("reversed box");
}
// Input: spec: finite domain and one positive cell count per state coordinate.
// Output: M: product of cell counts, with M + 1 checked to fit the state/container representation.
// Throws: std::invalid_argument for invalid geometry/counts; std::overflow_error for count overflow.
StateId validateGridSpec(const GridSpec& spec) {
    validateBox(spec.domain);
    if (spec.cells_per_dimension.size() != spec.domain.lo.size()) throw std::invalid_argument("grid shape mismatch");
    StateId count = 1;
    for (std::size_t d = 0; d < spec.domain.lo.size(); ++d) {
        const double width = spec.domain.hi[d] - spec.domain.lo[d];
        if (!(width > 0) || !std::isfinite(width) || spec.cells_per_dimension[d] == 0)
            throw std::invalid_argument("invalid grid width/count");
        count = checkedMultiply(count, spec.cells_per_dimension[d]);
    }
    (void)checkedSize(checkedAdd(count, 1)); // OUT + internal states must fit.
    return count;
}
// Input: value: absolute-bound matrix; rows/columns: required shape.
// Output: No return value; confirms the shape and finite, nonnegative entries; value is unchanged.
// Throws: std::invalid_argument for incorrect shape, nonfinite entries or negative bounds.
void validateMatrix(const Matrix& value, std::size_t rows, std::size_t columns) {
    if (value.size() != rows) throw std::invalid_argument("matrix row mismatch");
    for (const auto& row : value) {
        validateVector(row, columns);
        for (double x : row) if (x < 0) throw std::invalid_argument("negative absolute derivative bound");
    }
}
// Input: bounds: absolute derivative matrices; n: positive state dimension; q: disturbance dimension,
//        possibly zero.
// Output: No return value; checks finite, nonnegative Dx (n x n) and Dw (n x q); bounds are unchanged.
// Throws: std::invalid_argument for invalid dimensions, shapes or entries.
void validateDerivativeBounds(const DerivativeBounds& bounds, std::size_t n, std::size_t q) {
    if (n == 0) throw std::invalid_argument("empty dynamics state dimension");
    validateMatrix(bounds.Dx, n, n);
    validateMatrix(bounds.Dw, n, q);
}
// Input: value: numerical-guarantee enum.
// Output: Static string "uncertified_float" or "validated_enclosure"; caller does not own it.
// Throws: std::invalid_argument for an unknown enum value.
const char* toString(NumericGuarantee value) {
    switch (value) {
    case NumericGuarantee::UncertifiedFloat: return "uncertified_float";
    case NumericGuarantee::ValidatedEnclosure: return "validated_enclosure";
    }
    throw std::invalid_argument("unknown numerical guarantee");
}
// Input: value: model-provenance enum.
// Output: Static string "hand_authored_fixture" or "generated_part1"; caller does not own it.
// Throws: std::invalid_argument for an unknown enum value.
const char* toString(Provenance value) {
    switch (value) {
    case Provenance::HandAuthoredFixture: return "hand_authored_fixture";
    case Provenance::GeneratedPart1: return "generated_part1";
    }
    throw std::invalid_argument("unknown provenance");
}
}
