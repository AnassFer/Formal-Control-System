#include "symctrl/factories.hpp"
#include "dynamics_validation.hpp"

namespace symctrl {
namespace {
constexpr long double a = 0.1L, b = 0.2L, heater = 30.0L;

class ThermostatDynamics final : public IDynamics {
public:
    std::size_t stateDimension() const noexcept override { return 1; }
    std::size_t inputDimension() const noexcept override { return 1; }
    std::size_t disturbanceDimension() const noexcept override { return 1; }

    Vector step(const Vector& x, const Vector& u, const Vector& w) const override {
        validateVector(x, 1);
        validateVector(u, 1);
        validateVector(w, 1);
        // Affine form avoids cancellation/overflow in w-x and heater-x.
        return {detail::checkedDynamicsValue((1-a-b*u[0])*x[0] + a*w[0] + b*heater*u[0])};
    }

    bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, 1, 1, 1);
        return 1-a-b*input[0] >= 0; // State coefficient; disturbance coefficient a is positive.
    }

    DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                           const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, 1, 1, 1);
        return {{{detail::checkedDynamicsValue(std::abs(1-a-b*input[0]))}}, {{static_cast<double>(a)}}};
    }
};
}

std::shared_ptr<const IDynamics> makeThermostatDynamics() {
    return std::make_shared<ThermostatDynamics>();
}
}
