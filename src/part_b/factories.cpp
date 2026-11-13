#include "symctrl/contracts/factories.hpp"
#include "symctrl/part_b/reachability_backends.hpp"
#include <utility>

namespace symctrl {
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics> dynamics,
    ReachabilityMethod method, ReachabilityOptions options) {
    if (!dynamics) throw std::invalid_argument("null dynamics handle");
    if (dynamics->stateDimension() == 0 || dynamics->inputDimension() == 0)
        throw std::invalid_argument("state and input dimensions must be positive");

    switch (method) {
    case ReachabilityMethod::CooperativeMonotone:
    case ReachabilityMethod::BoundedDerivative:
        break;
    default:
        throw std::invalid_argument("unknown reachability method");
    }
    if (options.require_validated)
        throw UnsupportedCapability("validated reachability arithmetic is unavailable");

    if (method == ReachabilityMethod::CooperativeMonotone)
        return detail::makeMonotoneReachability(std::move(dynamics));
    return detail::makeDerivativeReachability(std::move(dynamics));
}
}
