#include "symctrl/contracts/factories.hpp"
#include "symctrl/part_b/dynamics_helpers.hpp"

namespace symctrl {
namespace {
constexpr long double alpha = 0.45L, beta = 0.045L, gamma = 0.09L;
constexpr long double outsideTemperature = -1.0L, heaterTemperature = 50.0L;

long double diagonal(double input) { return 1-2*alpha-beta-gamma*input; }

class RingBuildingDynamics final : public IDynamics {
    const std::size_t rooms_;

public:
    explicit RingBuildingDynamics(std::size_t rooms) : rooms_(rooms) {}
    std::size_t stateDimension() const noexcept override { return rooms_; }
    std::size_t inputDimension() const noexcept override { return rooms_; }
    std::size_t disturbanceDimension() const noexcept override { return 0; }

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

    bool cooperativeOn(const Box& states, const Vector& input, const Box& disturbances) const override {
        detail::validateDynamicsBox(states, input, disturbances, rooms_, rooms_, 0);
        // Distinct neighbours have positive coefficient alpha for every N >= 3.
        for (double u : input) if (diagonal(u) < 0) return false;
        return true;
    }

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

std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms) {
    if (rooms < 3) throw std::invalid_argument("a ring building needs at least three rooms");
    if (rooms > Vector{}.max_size() || rooms > Matrix{}.max_size())
        throw std::length_error("building dimensions exceed container limits");
    return std::make_shared<RingBuildingDynamics>(rooms);
}
}
