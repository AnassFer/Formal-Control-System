#include "symctrl/contracts/factories.hpp"
namespace symctrl {
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid&, const IInputCatalog&,
    const IReachability&, const Box&, BuildOptions) { throw NotImplemented("Part B graph builder"); }
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid>, std::shared_ptr<const IInputCatalog>,
    std::shared_ptr<const ITransitionSystem>, ModelMetadata) { throw NotImplemented("Part B bundle producer"); }
}
