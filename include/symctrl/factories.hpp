#pragma once
#include "symctrl/dynamics.hpp"
#include "symctrl/model_bundle.hpp"
#include "symctrl/policy.hpp"
#include "symctrl/reachable_enclosure.hpp"
#include <filesystem>
namespace symctrl {
// Discretization: immutable owned results; caller arguments are copied/moved into implementations.
// Input: spec: domain and positive cell counts per state coordinate.
// Output: Shared immutable grid owning the supplied specification.
// Throws: std::invalid_argument for invalid geometry/counts; std::overflow_error for count overflow.
std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec);
// Input: values: nonempty ordered list of finite, equally sized, positive-dimensional vectors.
// Output: Shared immutable input catalog preserving order and duplicates.
// Throws: std::invalid_argument for invalid input vectors.
std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values);
// Input: per_dimension_values: nonempty finite axis values for each input coordinate.
// Output: Shared immutable catalog of all combinations, with coordinate 0 varying fastest.
// Throws: std::invalid_argument for invalid axes; count overflow and allocation errors propagate.
std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector> per_dimension_values);
// Input: grid, inputs, policy: compatible views; measured_x: state vector; previous_memory: memory before
//        observation.
// Output: ConcreteDecision with located state, action ID, owned physical input vector and updated policy
//         memory.
// Throws: std::out_of_range for OUT, invalid action or missing policy entry; measurement/policy errors
//         propagate.
ConcreteDecision refineDecision(const IStateGrid& grid, const IInputCatalog& inputs,
                                const ISymbolicPolicy& policy, const Vector& measured_x,
                                PolicyMemory previous_memory = {});
// Example dynamics and general abstraction. Reachability retains dynamics;
// the graph owns its output independently of all inputs.
// Input: None; uses fixed a = 0.1, b = 0.2 and heater temperature 30.
// Output: Shared immutable thermostat dynamics with dimensions n = m = q = 1.
std::shared_ptr<const IDynamics> makeThermostatDynamics();
// Input: rooms: number of rooms, at least 3; thermal constants are fixed by the example.
// Output: Shared immutable circular-building dynamics with n = m = rooms and q = 0.
// Throws: std::invalid_argument for fewer than 3 rooms; std::length_error for unrepresentable container
//         dimensions.
std::shared_ptr<const IDynamics> makeRingBuildingDynamics(std::size_t rooms);
// Input: dynamics: nonnull model; method: enclosure algorithm; options: requested numerical guarantee.
// Output: Shared immutable enclosure backend retaining dynamics; current methods report UncertifiedFloat.
// Throws: std::invalid_argument for invalid model/method; UnsupportedCapability if validated arithmetic is
//         requested.
std::shared_ptr<const IReachability> makeReachability(std::shared_ptr<const IDynamics> dynamics,
    ReachabilityMethod method, ReachabilityOptions options = {});
// Input: grid, inputs, reachability: compatible views; disturbance: q-dimensional box; options: graph
//        budgets.
// Output: Shared immutable graph owning all state/action successor rows, including universal OUT rows and
//         build statistics.
// Throws: std::invalid_argument for inconsistent data; std::length_error for exceeded budgets;
//         arithmetic/backend errors propagate.
std::shared_ptr<const ITransitionSystem> buildSymbolicModel(const IStateGrid& grid,
    const IInputCatalog& inputs, const IReachability& reachability, const Box& disturbance,
    BuildOptions options = {});
// Input: grid, inputs, transitions: shared immutable handles; metadata: matching model description.
// Output: Validated ModelBundle retaining shared ownership of the three views and owning its metadata.
// Throws: Bundle consistency or checked-count validation errors propagate.
ModelBundle assembleModelBundle(std::shared_ptr<const IStateGrid> grid,
    std::shared_ptr<const IInputCatalog> inputs, std::shared_ptr<const ITransitionSystem> transitions,
    ModelMetadata metadata);
struct ExportOptions { std::uint64_t max_transition_rows = 10000000; };
// Input: bundle: complete consistent model; directory: export destination; options: maximum emitted
//        transition rows.
// Output: No return value; writes/replaces manifest.json, cells.csv, inputs.csv and transitions.csv using
//         staged publication.
// Throws: Validation, row-budget or filesystem errors; publication attempts rollback and retains backups if
//         rollback fails.
void exportModelPackage(const ModelBundle& bundle, const std::filesystem::path& directory,
                        ExportOptions options = {});
}
