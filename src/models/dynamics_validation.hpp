#pragma once
#include "symctrl/types.hpp"
#include <cmath>
#include <limits>

namespace symctrl::detail {
// Input: states/input/disturbances: requested model arguments; n/m/q: required dimensions, with q possibly
//        zero.
// Output: No return value; checks finite, dimensionally consistent state/input/disturbance data without
//         modifying it.
// Throws: std::invalid_argument for invalid shapes, coordinates or box bounds.
inline void validateDynamicsBox(const Box& states, const Vector& input, const Box& disturbances,
                                std::size_t n, std::size_t m, std::size_t q) {
    validateBox(states);
    validateVector(states.lo, n);
    validateVector(input, m);
    validateBox(disturbances, true);
    validateVector(disturbances.lo, q);
}

// Input: value: long-double dynamics result.
// Output: The result rounded to double, provided it is finite and inside the double range.
// Throws: std::overflow_error when value is nonfinite or outside the representable double range.
inline double checkedDynamicsValue(long double value) {
    const auto limit = static_cast<long double>(std::numeric_limits<double>::max());
    if (!std::isfinite(value) || value < -limit || value > limit)
        throw std::overflow_error("dynamics result is not representable as a finite double");
    return static_cast<double>(value);
}
}
