#include "symctrl/contracts/factories.hpp"
namespace symctrl {
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics>,
    ReachabilityMethod, ReachabilityOptions) { throw NotImplemented("Part B reachability backend"); }
}
