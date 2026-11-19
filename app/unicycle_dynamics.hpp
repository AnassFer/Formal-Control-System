#pragma once

#include "symctrl/dynamics.hpp"

namespace robot_app {

constexpr double kPi = 3.14159265358979323846;

double wrapAngle(double theta);

class UnicycleDynamics final : public symctrl::IDynamics {
public:
    explicit UnicycleDynamics(double tau = 1.0);

    std::size_t stateDimension() const noexcept override;
    std::size_t inputDimension() const noexcept override;
    std::size_t disturbanceDimension() const noexcept override;

    symctrl::Vector step(const symctrl::Vector& x,
                         const symctrl::Vector& u,
                         const symctrl::Vector& w) const override;

    bool cooperativeOn(const symctrl::Box&,
                       const symctrl::Vector&,
                       const symctrl::Box&) const override;

    symctrl::DerivativeBounds absoluteJacobianBounds(const symctrl::Box&,
                                                     const symctrl::Vector& input,
                                                     const symctrl::Box&) const override;

private:
    double tau_;
};

} // namespace robot_app