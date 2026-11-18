#pragma once
#include "symctrl/dynamics.hpp"
#include "symctrl/reachable_enclosure.hpp"
#include <memory>

namespace symctrl::detail {
// Internal construction after the public factory validates the dynamics/options.
std::shared_ptr<const IReachability> makeMonotoneReachability(
    std::shared_ptr<const IDynamics> dynamics);
std::shared_ptr<const IReachability> makeDerivativeReachability(
    std::shared_ptr<const IDynamics> dynamics);
}
