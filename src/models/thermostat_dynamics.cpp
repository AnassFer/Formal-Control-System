#include "symctrl/factories.hpp"
#include "dynamics_validation.hpp"

namespace symctrl {
namespace {
constexpr long double a = 0.1L, b = 0.2L, heater = 30.0L;

class ThermostatDynamics final : public IDynamics {
public:
    // Input: None; uses the thermostat model definition.
    // Output: 1: the room temperature coordinate.
    std::size_t stateDimension() const noexcept override { return 1; }
    // Input: None; uses the thermostat model definition.
    // Output: 1: the heater input coordinate.
    std::size_t inputDimension() const noexcept override { return 1; }
    // Input: None; uses the thermostat model definition.
    // Output: 1: the outside-temperature disturbance coordinate.
    std::size_t disturbanceDimension() const noexcept override { return 1; }

    // Input: x/u/w: finite one-coordinate vectors for room temperature, heater input and outside temperature.
    // Output: One-coordinate next temperature: (1-a-b*u)*x + a*w + b*heater*u.
    // Throws: std::invalid_argument for invalid vectors; std::overflow_error for an unrepresentable result.
    Vector step(const Vector& x, const Vector& u, const Vector& w) const override {
        validateVector(x, 1);
        validateVector(u, 1);
        validateVector(w, 1);
        // Affine form avoids cancellation/overflow in w-x and heater-x.
        return {detail::checkedDynamicsValue((1-a-b*u[0])*x[0] + a*w[0] + b*heater*u[0])};
    }

    // Input: states/disturbances: finite one-dimensional boxes; input: finite one-coordinate heater setting.
    // Output: True when 1-a-b*input[0] >= 0; the disturbance coefficient a is already positive.
    // Throws: std::invalid_argument for invalid request dimensions or bounds.
    bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, 1, 1, 1);
        return 1-a-b*input[0] >= 0; // State coefficient; disturbance coefficient a is positive.
    }

    // Input: states/disturbances: finite one-dimensional boxes; input: fixed one-coordinate heater setting.
    // Output: DerivativeBounds with Dx = [[abs(1-a-b*u)]] and Dw = [[a]].
    // Throws: std::invalid_argument for invalid requests; std::overflow_error for an unrepresentable state
    //         bound.
    DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                           const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, 1, 1, 1);
        return {{{detail::checkedDynamicsValue(std::abs(1-a-b*input[0]))}}, {{static_cast<double>(a)}}};
    }
};
}

// Input: None; uses fixed a = 0.1, b = 0.2 and heater temperature 30.
// Output: Shared immutable thermostat dynamics with dimensions n = m = q = 1.
std::shared_ptr<const IDynamics> makeThermostatDynamics() {
    return std::make_shared<ThermostatDynamics>();
}
}
