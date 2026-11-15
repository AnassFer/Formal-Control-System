#pragma once
#include "symctrl/contracts/dynamics.hpp"
#include "symctrl/contracts/model_bundle.hpp"
#include "symctrl/contracts/policy.hpp"
#include "symctrl/contracts/reachability.hpp"
#include <filesystem>
namespace symctrl {
// Part A: immutable owned results; caller arguments are copied/moved into implementations.
std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec);
std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values);
std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector> per_dimension_values);
ConcreteDecision refineDecision(const IStateGrid& grid, const IInputCatalog& inputs,
                                const ISymbolicPolicy& policy, const Vector& measured_x,
                                PolicyMemory previous_memory = {});
// Part B: reachability retains dynamics; graph owns its output independently of all inputs.
std::shared_ptr<const IDynamics> makeThermostatDynamics();
std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms);
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics> dynamics,
    ReachabilityMethod method, ReachabilityOptions options = {});
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid& grid,
    const IInputCatalog& inputs, const IReachability& reachability, const Box& disturbance,
    BuildOptions options = {});
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid> grid,
    std::shared_ptr<const IInputCatalog> inputs, std::shared_ptr<const ITransitionSystem> transitions,
    ModelMetadata metadata);
struct ExportOptions { std::uint64_t max_transition_rows = 10000000; };
void exportModelPackage(const ModelBundle& bundle, const std::filesystem::path& directory,
                        ExportOptions options = {});
}
