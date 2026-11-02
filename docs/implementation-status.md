# Baseline implementation-status manifest

Version 1.0.0 at `part1-contract-v1`. This file describes the frozen initial baseline. Owners record later progress in their own `docs/part_a/` and `docs/part_b/` notes; do not concurrently edit this shared manifest to track every feature commit.

| Feature / exact public symbol | Owner / defining file | Baseline status | Required checks |
|---|---|---|---|
| Value/error/version declarations | shared / contracts/types.hpp | contract complete | contracts.shapes, contracts.metadata |
| Shape/count/derivative metadata validation | shared / src/common/validation.cpp | complete trivial helper | contracts.shapes, contracts.counts |
| Successor membership/nonempty/iteration | shared / src/common/successors.cpp | complete representation helper | contracts.successors |
| Bundle view/metadata consistency | shared / src/common/model_bundle.cpp | complete trivial helper | handoff.consistency |
| makeUniformGrid | A / src/part_a/uniform_grid.cpp | throwing stub, no encoding/geometry algorithm | part_a.indexing, geometry, invalid |
| makeInputCatalog, makeCartesianInputs | A / src/part_a/input_catalog.cpp | throwing stubs | part_a.inputs |
| refineDecision | A / src/part_a/policy_adapter.cpp | throwing stub | part_a.policy |
| makeThermostatDynamics | B / src/part_b/thermostat_dynamics.cpp | throwing stub | part_b.thermostat |
| makeRingBuildingDynamics | B / src/part_b/ring_building_dynamics.cpp | throwing stub | part_b.building |
| makeReachability | B / src/part_b/factories.cpp, method files reserved | throwing stub; both methods missing | part_b.thermostat, affine, capabilities |
| ITransitionSystem concrete storage | B / src/part_b/explicit_transition_system.cpp | reserved empty translation unit | part_b.graph |
| buildSymbolicModel | B / src/part_b/model_builder.cpp | throwing stub | part_b.graph, boundaries, limits |
| assembleModelBundle | B / src/part_b/model_builder.cpp | throwing producer stub | part_b.export |
| exportModelPackage | B / src/part_b/model_export.cpp | throwing stub; no importer/exporter algorithm | part_b.export |
| CLI | shared / apps/main.cpp | compiled orchestration skeleton; calls stubs, exits 2 | documented thermostat commands |
| Real A+B integration | shared / tests/integration/part1.cpp | real assertions written, pending | integration.thermostat |
| Table grid/input/graph adapters | shared / fixtures/support/fixture.cpp | hand_authored_fixture complete, test-only | handoff.thermostat, choice |
| Versioned fixture model/spec files | shared / fixtures/part1_v1/ | complete fixed samples, no generated output | handoff.packages, optional package checker |
| Fixture-only consumer | shared / examples/part2_fixture_consumer.cpp | working with no A/B link | handoff.consumer |
| Strict readiness / path ownership | shared / tools/ | working optional developer tools | see bootstrap-validation.md |
| Part 2 synthesis, safety/reachability/products | reserved Part 2 trees | reserved, no algorithms | no synthesis tests claimed |

Full bootstrap CTest has 23 cases: 9 contracts/handoff pass, 5 A+8 B+1 production integration skip on NotImplemented only. No production algorithm is partly implemented to remove a skip. Strict foundation readiness passes with zero skips; A/B/integration readiness correctly fails. Handoff success must not be described as production integration success.
