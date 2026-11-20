#pragma once
#include "symctrl/types.hpp"
namespace symctrl {
struct DerivativeBounds { Matrix Dx; Matrix Dw; };
// Input: bounds: absolute derivative matrices; n: positive state dimension; q: disturbance dimension,
//        possibly zero.
// Output: No return value; checks finite, nonnegative Dx (n x n) and Dw (n x q); bounds are unchanged.
// Throws: std::invalid_argument for invalid dimensions, shapes or entries.
void validateDerivativeBounds(const DerivativeBounds& bounds, std::size_t n, std::size_t q);
class IDynamics {
public:
    // Input: None; called when this dynamics model is destroyed.
    // Output: No return value; permits destruction of the concrete object through this interface.
    virtual ~IDynamics() = default;
    // Input: None; reads this dynamics model.
    // Output: Number n of state coordinates.
    virtual std::size_t stateDimension() const noexcept = 0;
    // Input: None; reads this dynamics model.
    // Output: Number m of physical input coordinates.
    virtual std::size_t inputDimension() const noexcept = 0;
    // Input: None; reads this dynamics model.
    // Output: Number q of disturbance coordinates; q may be zero.
    virtual std::size_t disturbanceDimension() const noexcept = 0;
    // Input: x: n-coordinate state; u: m-coordinate input; w: q-coordinate disturbance.
    // Output: The n-coordinate next state f(x, u, w).
    // Throws: Model-specific validation, capability or arithmetic errors propagate.
    virtual Vector step(const Vector& x, const Vector& u, const Vector& w) const = 0;
    // True only with an analytic cooperative-monotonicity justification over both boxes.
    // Input: states: n-dimensional box; input: fixed m-coordinate input; disturbances: q-dimensional box.
    // Output: True only when nondecreasing dependence on state and disturbance is justified over both entire
    //         boxes.
    // Throws: Invalid requests or unsupported model operations may raise exceptions.
    virtual bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const = 0;
    // Fixed input; componentwise absolute bounds valid over the entire requested boxes.
    // UnsupportedCapability if unavailable; never a centre-Jacobian fallback.
    // Input: states: n-dimensional box; input: fixed m-coordinate input; disturbances: q-dimensional box.
    // Output: Nonnegative absolute bounds Dx (n x n) and Dw (n x q), valid throughout both boxes.
    // Throws: UnsupportedCapability if full-box bounds are unavailable; validation/arithmetic errors may
    //         propagate.
    virtual DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                                   const Box& disturbances) const = 0;
};
}
