#include "unicycle_dynamics.hpp"

#include <cmath>
#include <stdexcept>

namespace robot_app {

double wrapAngle(double theta) {
    theta = std::fmod(theta + kPi, 2.0 * kPi);
    if (theta < 0.0) {
        theta += 2.0 * kPi;
    }
    return theta - kPi;
}

UnicycleDynamics::UnicycleDynamics(double tau) : tau_(tau) {
    if (!std::isfinite(tau_) || tau_ <= 0.0) {
        throw std::invalid_argument("UnicycleDynamics: tau must be strictly positive and finite");
    }
}

std::size_t UnicycleDynamics::stateDimension() const noexcept { return 3; }
std::size_t UnicycleDynamics::inputDimension() const noexcept { return 2; }
std::size_t UnicycleDynamics::disturbanceDimension() const noexcept { return 3; }

symctrl::Vector UnicycleDynamics::step(const symctrl::Vector& x,
                                       const symctrl::Vector& u,
                                       const symctrl::Vector& w) const {
    if (x.size() != 3 || u.size() != 2 || w.size() != 3) {
        throw std::invalid_argument("UnicycleDynamics::step: invalid vector dimensions");
    }
    return {
        x[0] + tau_ * (u[0] * std::cos(x[2]) + w[0]),
        x[1] + tau_ * (u[0] * std::sin(x[2]) + w[1]),
        wrapAngle(x[2] + tau_ * (u[1] + w[2]))
    };
}

bool UnicycleDynamics::cooperativeOn(const symctrl::Box&,
                                     const symctrl::Vector&,
                                     const symctrl::Box&) const {
    return false;
}

symctrl::DerivativeBounds UnicycleDynamics::absoluteJacobianBounds(const symctrl::Box&,
                                                                   const symctrl::Vector& input,
                                                                   const symctrl::Box&) const {
    if (input.size() != 2) {
        throw std::invalid_argument("UnicycleDynamics::absoluteJacobianBounds: invalid input size");
    }
    const double tauAbsU1 = tau_ * std::abs(input[0]);
    symctrl::DerivativeBounds bounds;
    bounds.Dx = {
        {1.0, 0.0, tauAbsU1},
        {0.0, 1.0, tauAbsU1},
        {0.0, 0.0, 1.0}
    };
    bounds.Dw = {
        {tau_, 0.0,  0.0},
        {0.0,  tau_, 0.0},
        {0.0,  0.0,  tau_}
    };
    return bounds;
}

} // namespace robot_app