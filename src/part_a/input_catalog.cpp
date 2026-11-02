#include "symctrl/contracts/factories.hpp"
namespace symctrl {
std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector>) { throw NotImplemented("Part A explicit inputs"); }
std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector>) { throw NotImplemented("Part A Cartesian inputs"); }
}
