#include "symctrl/part_b/reachability_backends.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace symctrl {
namespace {
double finiteResult(double value) {
    if (!std::isfinite(value)) throw std::overflow_error("reachability arithmetic overflow");
    return value;
}

struct CentreRadius { Vector centre; Vector radius; };

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

void addRadius(double& radius, const Vector& bounds, const Vector& halfWidths) {
    for (std::size_t j = 0; j < bounds.size(); ++j)
        radius = finiteResult(radius + finiteResult(bounds[j]*halfWidths[j]));
}

class DerivativeReachability final : public IReachability {
    const std::shared_ptr<const IDynamics> dynamics_;

public:
    explicit DerivativeReachability(std::shared_ptr<const IDynamics> dynamics)
        : dynamics_(std::move(dynamics)) {}

    std::size_t stateDimension() const noexcept override { return dynamics_->stateDimension(); }
    std::size_t inputDimension() const noexcept override { return dynamics_->inputDimension(); }
    std::size_t disturbanceDimension() const noexcept override { return dynamics_->disturbanceDimension(); }
    NumericGuarantee numericGuarantee() const noexcept override {
        return NumericGuarantee::UncertifiedFloat;
    }

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
std::shared_ptr<const IReachability> makeDerivativeReachability(
    std::shared_ptr<const IDynamics> dynamics) {
    return std::make_shared<DerivativeReachability>(std::move(dynamics));
}
}
}
