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
    explicit NotImplemented(const std::string& feature) : std::logic_error(feature + ": not implemented") {}
};
class UnsupportedCapability : public std::logic_error {
public:
    using std::logic_error::logic_error;
};
enum class NumericGuarantee { UncertifiedFloat, ValidatedEnclosure };
enum class Provenance { HandAuthoredFixture, GeneratedPart1 };
struct Box { Vector lo; Vector hi; };
struct GridSpec { Box domain; GridIndex cells_per_dimension; };

void validateVector(const Vector& value, std::size_t dimension);
void validateBox(const Box& box, bool allow_empty_disturbance = false);
StateId validateGridSpec(const GridSpec& spec); // Returns checked M; no grid algorithm.
void validateMatrix(const Matrix& value, std::size_t rows, std::size_t columns);
std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b);
std::uint64_t checkedMultiply(std::uint64_t a, std::uint64_t b);
std::size_t checkedSize(std::uint64_t value);
const char* toString(NumericGuarantee value);
const char* toString(Provenance value);
} // namespace symctrl
