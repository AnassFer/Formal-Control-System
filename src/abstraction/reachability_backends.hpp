#pragma once
#include "symctrl/dynamics.hpp"
#include "symctrl/reachable_enclosure.hpp"
#include <memory>

namespace symctrl::detail {
// Internal construction after the public factory validates the dynamics/options.
// Input: dynamics: nonnull shared model validated by makeReachability.
// Output: Shared immutable cooperative-monotone backend retaining the model.
std::shared_ptr<const IReachability> makeMonotoneReachability(
    std::shared_ptr<const IDynamics> dynamics);
// Input: dynamics: nonnull shared model validated by makeReachability.
// Output: Shared immutable bounded-derivative backend retaining the model.
std::shared_ptr<const IReachability> makeDerivativeReachability(
    std::shared_ptr<const IDynamics> dynamics);
}
