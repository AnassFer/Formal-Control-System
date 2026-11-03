# Current template status

This document describes the existing scaffold and the intended general stages. No production algorithm, public signature, schema, build graph or test is changed by the documentation correction. The historical part1-contract-v1 tag remains fixed.

| Component | Owner | Current status |
|---|---|---|
| Generic value types, errors and public interfaces | Shared | Compilable contracts |
| Shape/count/derivative-bound validation | Shared | Completed small helpers |
| Successor validation/membership/nonemptiness/iteration | Shared | Completed representation helpers |
| ModelBundle consistency | Shared | Completed handle/metadata checks; not graph certification |
| Uniform grid, indexing/geometry/point abstraction | A | Production stub |
| Explicit and Cartesian inputs | A | Production stubs |
| Policy refinement with memory | A | Production stub |
| Generic cooperative and bounded-derivative reachability | B | Production factory stub and reserved method files |
| Sparse transition storage and generic graph builder | B | Reserved/stub |
| Bundle production and generic package export | B | Production stubs |
| General specification/synthesis API | Part 2/shared coordination | Model/policy boundary exists; exact synthesis factories/results still need agreement |
| General controller synthesis | Part 2 | Reserved; no algorithm implementation |
| Fixed graph/input/geometry fixtures | Shared | Working literal test/example data |
| Fixture-only consumer and standalone client | Shared | Working without A/B libraries |
| Mobile-robot end-to-end example | Later integration | Not implemented; follows the general engine |
| Legacy example-specific v1 scaffolding | Historical compatibility | Unchanged code; not the definition of generic completion |

## Checks currently available

The original default suite registers 23 cases: 4 contracts, 5 A, 8 B, 1 integration and 5 handoff. At bootstrap, 9 foundation/handoff checks passed and 14 production cases skipped on NotImplemented. Those are scaffold results, not a complete paper implementation.

A's responsibilities remain unchanged. B needs meaningful model-independent coverage as implementation proceeds. The registry has no Part 2 synthesis or later robot acceptance suite. Add that coverage through a coordinated implementation/test update before claiming the relevant stage ready. Do not weaken existing checks to fit a faulty implementation.

Owner progress belongs in docs/part_a/, docs/part_b/ and docs/part2/. Full data flow and module paths are in [the blueprint](part1-final-structure.md); current scope is in [scope-correction.md](scope-correction.md).
