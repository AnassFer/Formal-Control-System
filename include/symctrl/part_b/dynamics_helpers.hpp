#pragma once
#include "symctrl/contracts/types.hpp"
#include <cmath>
#include <limits>

namespace symctrl::detail {
inline void validateDynamicsBox(const Box& states, const Vector& input, const Box& disturbances,
                                std::size_t n, std::size_t m, std::size_t q) {
    validateBox(states);
    validateVector(states.lo, n);
    validateVector(input, m);
    validateBox(disturbances, true);
    validateVector(disturbances.lo, q);
}

inline double checkedDynamicsValue(long double value) {
    const auto limit = static_cast<long double>(std::numeric_limits<double>::max());
    if (!std::isfinite(value) || value < -limit || value > limit)
        throw std::overflow_error("dynamics result is not representable as a finite double");
    return static_cast<double>(value);
}
}
