#pragma once
#include "symctrl/contracts/grid.hpp"
#include "symctrl/contracts/inputs.hpp"
#include "symctrl/contracts/transitions.hpp"
#include <map>
#include <memory>
namespace symctrl {
struct ModelMetadata {
    std::string contract_version = kContractVersion;
    std::string schema_version = kSchemaVersion;
    std::string model_id;
    std::map<std::string, double> model_parameters;
    Provenance provenance = Provenance::HandAuthoredFixture;
    std::string source;
    std::size_t state_dimension = 0, input_dimension = 0, disturbance_dimension = 0;
    StateId internal_states = 0;
    InputId input_count = 0;
    StateId outside_state_id = kOutside;
    NumericGuarantee numeric_guarantee = NumericGuarantee::UncertifiedFloat;
    GridSpec grid;
    Box disturbance;
};
struct ModelBundle {
    std::shared_ptr<const IStateGrid> grid;
    std::shared_ptr<const IInputCatalog> inputs;
    std::shared_ptr<const ITransitionSystem> transitions;
    ModelMetadata metadata;
};
// Trivial handle/metadata consistency only, not an exhaustive graph certification.
void validateBundle(const ModelBundle& bundle);
}
