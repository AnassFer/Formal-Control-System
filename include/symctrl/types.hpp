#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace symctrl {
using StateId = std::uint64_t;
using InputId = std::uint64_t;
using Vector = std::vector<double>;
using Matrix = std::vector<Vector>; // Row-major: matrix[row][column].
using GridIndex = std::vector<std::uint32_t>;
inline constexpr StateId kOutside = 0;
inline constexpr const char* kContractVersion = "1.0.0";
inline constexpr const char* kSchemaVersion = "1.0.0";

class NotImplemented : public std::logic_error {
public:
    // Input: feature: name of the unavailable feature.
    // Output: Constructs an exception with the message "feature: not implemented"; no return value.
    explicit NotImplemented(const std::string& feature) : std::logic_error(feature + ": not implemented") {}
};
class UnsupportedCapability : public std::logic_error {
public:
    // Input: An error message accepted by a std::logic_error constructor.
    // Output: Inherits constructors that create an UnsupportedCapability exception; no return value.
    using std::logic_error::logic_error;
};
enum class NumericGuarantee { UncertifiedFloat, ValidatedEnclosure };
enum class Provenance { HandAuthoredFixture, GeneratedPart1 };
struct Box { Vector lo; Vector hi; };
struct GridSpec { Box domain; GridIndex cells_per_dimension; };

// Input: value: coordinates; dimension: required number of coordinates.
// Output: No return value; leaves value unchanged when its size and all coordinates are valid.
// Throws: std::invalid_argument for a dimension mismatch or NaN/infinite coordinate.
void validateVector(const Vector& value, std::size_t dimension);
// Input: box: lower/upper coordinate bounds; allow_empty_disturbance: whether zero dimensions are allowed.
// Output: No return value; confirms matching finite bounds with lo[d] <= hi[d], without modifying box.
// Throws: std::invalid_argument for invalid bounds or a disallowed empty box.
void validateBox(const Box& box, bool allow_empty_disturbance = false);
// Input: spec: finite domain and one positive cell count per state coordinate.
// Output: M: product of cell counts, with M + 1 checked to fit the state/container representation.
// Throws: std::invalid_argument for invalid geometry/counts; std::overflow_error for count overflow.
StateId validateGridSpec(const GridSpec& spec); // Returns checked M; no grid algorithm.
// Input: value: absolute-bound matrix; rows/columns: required shape.
// Output: No return value; confirms the shape and finite, nonnegative entries; value is unchanged.
// Throws: std::invalid_argument for incorrect shape, nonfinite entries or negative bounds.
void validateMatrix(const Matrix& value, std::size_t rows, std::size_t columns);
// Input: a, b: unsigned 64-bit counts.
// Output: Their sum a + b as an unsigned 64-bit count.
// Throws: std::overflow_error if the sum cannot be represented.
std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b);
// Input: a, b: unsigned 64-bit counts.
// Output: Their product a * b as an unsigned 64-bit count.
// Throws: std::overflow_error if the product cannot be represented.
std::uint64_t checkedMultiply(std::uint64_t a, std::uint64_t b);
// Input: value: unsigned 64-bit count.
// Output: The same count represented as std::size_t.
// Throws: std::overflow_error if value does not fit std::size_t.
std::size_t checkedSize(std::uint64_t value);
// Input: value: numerical-guarantee enum.
// Output: Static string "uncertified_float" or "validated_enclosure"; caller does not own it.
// Throws: std::invalid_argument for an unknown enum value.
const char* toString(NumericGuarantee value);
// Input: value: model-provenance enum.
// Output: Static string "hand_authored_fixture" or "generated_part1"; caller does not own it.
// Throws: std::invalid_argument for an unknown enum value.
const char* toString(Provenance value);
} // namespace symctrl
