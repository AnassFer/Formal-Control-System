#include "symctrl/factories.hpp"
#include "dynamics_validation.hpp"

namespace symctrl {
namespace {
constexpr long double alpha = 0.45L, beta = 0.045L, gamma = 0.09L;
constexpr long double outsideTemperature = -1.0L, heaterTemperature = 50.0L;

// Input: input: heater setting for one room.
// Output: Long-double self-coupling coefficient 1 - 2*alpha - beta - gamma*input.
long double diagonal(double input) { return 1-2*alpha-beta-gamma*input; }

class RingBuildingDynamics final : public IDynamics {
    const std::size_t rooms_;

public:
    // Input: rooms: valid room count already checked by makeRingBuildingDynamics.
    // Output: Constructs a model storing the room count; no return value.
    explicit RingBuildingDynamics(std::size_t rooms) : rooms_(rooms) {}
    // Input: None; reads this circular-building model.
    // Output: rooms_: one temperature coordinate per room.
    std::size_t stateDimension() const noexcept override { return rooms_; }
    // Input: None; reads this circular-building model.
    // Output: rooms_: one heater coordinate per room.
    std::size_t inputDimension() const noexcept override { return rooms_; }
    // Input: None; reads this circular-building model.
    // Output: 0: the model has no disturbance coordinates.
    std::size_t disturbanceDimension() const noexcept override { return 0; }

    // Input: x/u: finite rooms_-coordinate temperature/input vectors; w: empty disturbance vector.
    // Output: rooms_-coordinate next temperatures using circular neighbors and the fixed thermal constants.
    // Throws: std::invalid_argument for invalid vectors; std::overflow_error for an unrepresentable
    //         temperature.
    Vector step(const Vector& x, const Vector& u, const Vector& w) const override {
        validateVector(x, rooms_);
        validateVector(u, rooms_);
        validateVector(w, 0);
        Vector result(rooms_);
        for (std::size_t i = 0; i < rooms_; ++i) {
            const auto previous = i == 0 ? rooms_-1 : i-1;
            const auto next = i == rooms_-1 ? 0 : i+1;
            result[i] = detail::checkedDynamicsValue(diagonal(u[i])*x[i]
                + alpha*x[previous] + alpha*x[next] + beta*outsideTemperature
                + gamma*heaterTemperature*u[i]);
        }
        return result;
    }

    // Input: states: rooms_-dimensional box; input: rooms_ heater settings; disturbances: empty box.
    // Output: True when every room self-coupling coefficient is nonnegative; neighbor coefficients are
    //         positive.
    // Throws: std::invalid_argument for invalid request dimensions or bounds.
    bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, rooms_, rooms_, 0);
        // Distinct neighbours have positive coefficient alpha for every N >= 3.
        for (double u : input) if (diagonal(u) < 0) return false;
        return true;
    }

    // Input: states: rooms_-dimensional box; input: fixed heater settings; disturbances: empty box.
    // Output: Dx: rooms_ x rooms_ absolute self/neighbor coefficients; Dw: rooms_ empty rows (rooms_ x 0).
    // Throws: Validation, dimension-count overflow, allocation or unrepresentable coefficient errors
    //         propagate.
    DerivativeBounds absoluteJacobianBounds(const Box& states, const Vector& input,
                                           const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, rooms_, rooms_, 0);
        const auto entries = checkedMultiply(rooms_, rooms_);
        (void)checkedSize(checkedAdd(checkedMultiply(entries, sizeof(double)),
                                   checkedMultiply(rooms_, 2*sizeof(Vector))));
        DerivativeBounds bounds{Matrix(rooms_, Vector(rooms_, 0)), Matrix(rooms_)};
        for (std::size_t i = 0; i < rooms_; ++i) {
            bounds.Dx[i][i] = detail::checkedDynamicsValue(std::abs(diagonal(input[i])));
            bounds.Dx[i][i == 0 ? rooms_-1 : i-1] = static_cast<double>(alpha);
            bounds.Dx[i][i == rooms_-1 ? 0 : i+1] = static_cast<double>(alpha);
        }
        return bounds;
    }
};
}

// Input: rooms: number of rooms, at least 3; thermal constants are fixed by the example.
// Output: Shared immutable circular-building dynamics with n = m = rooms and q = 0.
// Throws: std::invalid_argument for fewer than 3 rooms; std::length_error for unrepresentable container
//         dimensions.
std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms) {
    if (rooms < 3) throw std::invalid_argument("a ring building needs at least three rooms");
    if (rooms > Vector{}.max_size() || rooms > Matrix{}.max_size())
        throw std::length_error("building dimensions exceed container limits");
    return std::make_shared<RingBuildingDynamics>(rooms);
}
}
