#include "reachability_backends.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace symctrl {
namespace {
// Input: value: intermediate floating-point result.
// Output: The same value when finite.
// Throws: std::overflow_error for NaN or infinity.
double finiteResult(double value) {
    if (!std::isfinite(value)) throw std::overflow_error("reachability arithmetic overflow");
    return value;
}

struct CentreRadius { Vector centre; Vector radius; };

// Input: box: already validated finite closed box, possibly zero-dimensional.
// Output: CentreRadius with one center and covering half-width per coordinate; empty vectors for an empty
//         box.
// Throws: std::overflow_error if a computed center or radius is nonfinite.
CentreRadius centreRadius(const Box& box) {
    CentreRadius result{Vector(box.lo.size()), Vector(box.lo.size())};
    for (std::size_t d = 0; d < box.lo.size(); ++d) {
        const double lo = box.lo[d], hi = box.hi[d];
        // Same-sign subtraction and opposite-sign half sums cannot overflow here.
        const double centre = std::signbit(lo) == std::signbit(hi)
            ? lo + (hi - lo)/2 : lo/2 + hi/2;
        result.centre[d] = finiteResult(centre);
        // Cover midpoint rounding asymmetry, including intervals of subnormal width.
        result.radius[d] = finiteResult(std::max(centre - lo, hi - centre));
    }
    return result;
}

// Input: radius: current accumulator by reference; bounds/halfWidths: equally sized nonnegative vectors.
// Output: No return value; updates radius by adding sum(bounds[j] * halfWidths[j]); earlier additions remain
//         if a later one fails.
// Throws: std::overflow_error for nonfinite multiplication or accumulation.
void addRadius(double& radius, const Vector& bounds, const Vector& halfWidths) {
    for (std::size_t j = 0; j < bounds.size(); ++j)
        radius = finiteResult(radius + finiteResult(bounds[j]*halfWidths[j]));
}

class DerivativeReachability final : public IReachability {
    const std::shared_ptr<const IDynamics> dynamics_;

public:
    // Input: dynamics: nonnull shared immutable model, already checked by the public factory.
    // Output: Constructs a backend retaining shared ownership of dynamics; no return value.
    explicit DerivativeReachability(std::shared_ptr<const IDynamics> dynamics)
        : dynamics_(std::move(dynamics)) {}

    // Input: None; reads the retained dynamics model.
    // Output: Number n of state coordinates in the retained dynamics.
    std::size_t stateDimension() const noexcept override { return dynamics_->stateDimension(); }
    // Input: None; reads the retained dynamics model.
    // Output: Number m of input coordinates in the retained dynamics.
    std::size_t inputDimension() const noexcept override { return dynamics_->inputDimension(); }
    // Input: None; reads the retained dynamics model.
    // Output: Number q of disturbance coordinates in the retained dynamics; may be zero.
    std::size_t disturbanceDimension() const noexcept override { return dynamics_->disturbanceDimension(); }
    // Input: None; reads this enclosure backend.
    // Output: NumericGuarantee::UncertifiedFloat; rounding errors are not certified.
    NumericGuarantee numericGuarantee() const noexcept override {
        return NumericGuarantee::UncertifiedFloat;
    }

    // Input: cell: closed state box; input: fixed input; disturbances: closed disturbance box, empty when q =
    //        0.
    // Output: Box centered at f(xc,u,wc) with radius Dx*rx + Dw*rw; arithmetic is uncertified.
    // Throws: Validation or model-capability errors; std::overflow_error for nonfinite enclosure arithmetic.
    Box enclose(const Box& cell, const Vector& input, const Box& disturbances) const override {
        validateBox(cell);
        validateVector(cell.lo, stateDimension());
        validateVector(input, inputDimension());
        validateBox(disturbances, true);
        validateVector(disturbances.lo, disturbanceDimension());

        const auto bounds = dynamics_->absoluteJacobianBounds(cell, input, disturbances);
        validateDerivativeBounds(bounds, stateDimension(), disturbanceDimension());
        const auto states = centreRadius(cell);
        const auto noise = centreRadius(disturbances);
        const auto centre = dynamics_->step(states.centre, input, noise.centre);
        validateVector(centre, stateDimension());

        Box result{Vector(stateDimension()), Vector(stateDimension())};
        for (std::size_t d = 0; d < stateDimension(); ++d) {
            double radius = 0;
            addRadius(radius, bounds.Dx[d], states.radius);
            addRadius(radius, bounds.Dw[d], noise.radius);
            result.lo[d] = finiteResult(centre[d] - radius);
            result.hi[d] = finiteResult(centre[d] + radius);
        }
        return result;
    }
};
}

namespace detail {
// Input: dynamics: nonnull shared model validated by makeReachability.
// Output: Shared immutable bounded-derivative backend retaining the model.
std::shared_ptr<const IReachability> makeDerivativeReachability(
    std::shared_ptr<const IDynamics> dynamics) {
    return std::make_shared<DerivativeReachability>(std::move(dynamics));
}
}
}
