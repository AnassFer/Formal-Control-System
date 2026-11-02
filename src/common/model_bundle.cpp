#include "symctrl/contracts/model_bundle.hpp"
#include <cmath>
namespace symctrl {
void validateBundle(const ModelBundle& b) {
    if (!b.grid || !b.inputs || !b.transitions) throw std::invalid_argument("null bundle handle");
    const auto& m = b.metadata;
    if (m.contract_version != kContractVersion || m.schema_version != kSchemaVersion)
        throw std::invalid_argument("unsupported bundle version");
    if (m.model_id.empty() || m.source.empty()) throw std::invalid_argument("missing provenance metadata");
    (void)toString(m.provenance);
    (void)toString(m.numeric_guarantee);
    const auto count = validateGridSpec(m.grid);
    if (m.state_dimension != m.grid.domain.lo.size() || m.state_dimension != b.grid->stateDimension()
        || count != m.internal_states || count != b.grid->internalStateCount()
        || count != b.transitions->internalStateCount() || m.outside_state_id != kOutside
        || m.input_dimension == 0 || m.input_dimension != b.inputs->inputDimension()
        || m.input_count == 0 || m.input_count != b.inputs->inputCount()
        || m.input_count != b.transitions->inputCount()
        || m.numeric_guarantee != b.transitions->numericGuarantee()
        || m.grid.domain.lo != b.grid->spec().domain.lo || m.grid.domain.hi != b.grid->spec().domain.hi
        || m.grid.cells_per_dimension != b.grid->spec().cells_per_dimension)
        throw std::invalid_argument("bundle metadata/views disagree");
    validateBox(m.disturbance, true);
    if (m.disturbance.lo.size() != m.disturbance_dimension) throw std::invalid_argument("disturbance dimension mismatch");
    for (const auto& p : m.model_parameters)
        if (p.first.empty() || !std::isfinite(p.second)) throw std::invalid_argument("invalid model parameter");
}
}
