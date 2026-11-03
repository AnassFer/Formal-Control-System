# General template scope

The latest user instruction governs this revision: keep the implementation of the paper general, prepare its first part and a base for its second part, and use a mobile robot only as a later end-to-end example. This revision changes documentation only.

## Stages and ownership

| Stage | Purpose | Ownership |
|---|---|---|
| Paper Part 1 / Section 1 | General discretisation, conservative abstraction, transition model and policy refinement | Development A and B |
| Paper Part 2 / Section 2 | General symbolic-controller synthesis using a model and a separate specification | Reserved Part 2 workspace/base |
| Later example | Demonstrate the completed reusable pipeline with a mobile robot | Integrated example work after the general pieces are ready |

A/B are development tracks within the first stage. A owns grids, input catalogues and runtime refinement. B owns general reachability backends, transition construction/storage/query/export and model bundles. The later synthesis stage consumes that model interface and returns a symbolic policy. No model-specific task defines the general engine.

A policy interface and adapter do not themselves implement controller synthesis. Paper Section 1 provides the abstraction/refinement foundation; Section 2 provides synthesis. The full project remains general across both stages, with the robot added after the engine rather than used to hard-code it.

## Repository audit

| Area | Finding |
|---|---|
| A interfaces | Runtime dimensions, generic boxes, state/action IDs and supplied-policy refinement are appropriate; A's responsibilities remain unchanged |
| IDynamics | Generic dimensions, step operation and model-supplied mathematical capabilities |
| IReachability | Generic cell/input/disturbance enclosure; no fixed physical interpretation is required |
| makeReachability | Accepts an injected dynamics interface and selected general method |
| ITransitionSystem / buildSymbolicModel | Generic grid/input/enclosure-driven transition construction and successor queries |
| ModelBundle / file schema | Model-independent geometry/input/graph metadata and exchange format |
| ISymbolicPolicy | General finite-memory decision interface; a synthesis implementation is still absent |
| Production implementation | A/B factories remain stubs; no finished general engine is claimed |
| Part 2 | Reserved workspace, public model/policy boundary and fixture consumer exist; synthesis APIs/algorithms remain pending |
| Later robot example | Not implemented; it must use the general engine and handle periodic heading soundly |

The existing v1 scaffold also contains legacy example-specific declarations/tests/CLI wiring. This documentation-only correction neither changes those files nor presents them as the required design. A future coordinated code update must separate example adapters from the reusable core and extend generic acceptance coverage without silently breaking existing APIs.

## General data flow

```text
Supplied dynamics + domains + input vectors + disturbances
  -> A: grid and input catalogue
  -> B: reachability enclosure and transition model
Transition model + separate specification
  -> Part 2: general synthesis -> symbolic policy
Measured state + policy + previous memory
  -> A: physical input and updated memory
```

Models implement IDynamics or provide a compatible IReachability backend. Generic algorithms do not inspect a model name, embed physical constants or assume a fixed coordinate count. Unsupported mathematical capabilities fail explicitly.

## Documentation-only boundary

No production C++, public signature, schema, CMake file, test source or readiness registry is changed. The original tag part1-contract-v1 stays attached to its original commit. Existing branch refs and the separate B worktree stay untouched. No commit, push, benchmark or implementation is performed.

The old 23-case suite is scaffold evidence, not full-paper acceptance. It lacks a generic synthesis suite and later robot coverage. The next implementation step needs a coordinated contract/test update where the current generic interfaces are insufficient; do not claim planned declarations exist.

The earlier handoff archive is historical and must not be used as current guidance. Read the corrected [PART2_HANDOFF.md](../PART2_HANDOFF.md) and [quickstart](part2-quickstart.md) for the model-independent Part 2 base.
