#pragma once
#include "symctrl/contracts/dynamics.hpp"
#include "symctrl/contracts/reachability.hpp"
#include <memory>

namespace symctrl::detail {
// Internal construction after the public factory validates the dynamics/options.
std::shared_ptr<const IReachability> makeMonotoneReachability(
    std::shared_ptr<const IDynamics> dynamics);
}
