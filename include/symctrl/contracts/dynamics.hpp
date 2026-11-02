#pragma once
#include "symctrl/contracts/types.hpp"
namespace symctrl {
struct DerivativeBounds { Matrix Dx; Matrix Dw; };
void validateDerivativeBounds(const DerivativeBounds& bounds, std::size_t n, std::size_t q);
class IDynamics {
public:
    virtual ~IDynamics() = default;
    virtual std::size_t stateDimension() const noexcept = 0;
    virtual std::size_t inputDimension() const noexcept = 0;
    virtual std::size_t disturbanceDimension() const noexcept = 0;
    virtual Vector step(const Vector& x, const Vector& u, const Vector& w) const = 0;
    // True only with an analytic cooperative-monotonicity justification over both boxes.
    virtual bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const = 0;
    // Fixed input; componentwise absolute bounds valid over the entire requested boxes.
    // UnsupportedCapability if unavailable; never a centre-Jacobian fallback.
    virtual DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                                   const Box& disturbances) const = 0;
};
}
