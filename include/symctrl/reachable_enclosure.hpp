#pragma once
#include "symctrl/types.hpp"
namespace symctrl {
enum class ReachabilityMethod { CooperativeMonotone, BoundedDerivative };
struct ReachabilityOptions {
    bool require_validated = false; // Reject uncertified backends when true; no tolerance fudge.
};
class IReachability {
public:
    // Input: None; called when this enclosure backend is destroyed.
    // Output: No return value; permits destruction of the concrete object through this interface.
    virtual ~IReachability() = default;
    // Input: None; reads the retained dynamics model.
    // Output: Number n of state coordinates in the retained dynamics.
    virtual std::size_t stateDimension() const noexcept = 0;
    // Input: None; reads the retained dynamics model.
    // Output: Number m of input coordinates in the retained dynamics.
    virtual std::size_t inputDimension() const noexcept = 0;
    // Input: None; reads the retained dynamics model.
    // Output: Number q of disturbance coordinates in the retained dynamics; may be zero.
    virtual std::size_t disturbanceDimension() const noexcept = 0;
    // Input: None; reads this enclosure backend.
    // Output: The numerical-guarantee enum describing the backend arithmetic.
    virtual NumericGuarantee numericGuarantee() const noexcept = 0;
    // Input: cell: n-dimensional closed box; input: fixed m-coordinate vector; disturbance_box: q-dimensional
    //        box.
    // Output: An n-dimensional box covering one-step outcomes throughout the supplied boxes under the backend
    //         assumptions.
    // Throws: Validation, unsupported-capability or arithmetic errors propagate.
    virtual Box enclose(const Box& cell, const Vector& input, const Box& disturbance_box) const = 0;
};
}
