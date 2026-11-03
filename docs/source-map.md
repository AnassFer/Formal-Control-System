# Paper sections and general implementation stages

The latest user direction is a general template for paper Part 1 and a base for Part 2. Keep both stages reusable. The mobile robot is a later end-to-end example, after the underlying engine. See [the scope note](scope-correction.md).

| Source | Role in the general project |
|---|---|
| Paper 1.1.1, printed 3 / PDF 4 | A: state/input discretisation, abstraction q(x), input concretisation p(action) |
| Paper 1.1.2, printed 4 / PDF 5 | B: conservative reachable enclosures and transition construction |
| Paper 1.1.3, printed 5 / PDF 6 | B: cooperative monotonicity and bounded-derivative methods |
| Paper 1.2, printed 6 / PDF 7 | General policy/memory interface and A's runtime refinement |
| Paper 2.1, printed 7–9 / PDF 8–10 | Subsequent general safety synthesis; optimisation is a separate extension |
| Paper 2.2, printed 9–10 / PDF 10–11 | Subsequent general reachability and finite-automaton specifications |
| Paper Section 3, printed 11–12 / PDF 12–13 | Later mobile-robot example consuming the general engine |
| Paper Section 4 | Advanced topics; not automatically required for the template |

A/B are development owners within the first stage. The policy interface in Section 1 does not mean synthesis is already implemented. The article supplies the mathematics; the original bootstrap prompt is historical and later user directions govern current scope. Source-file locations are in [reference/README.md](reference/README.md).

## General dynamics and enclosures

The model has discrete time and continuous state: x_next=f(x,u,w), with n state, m input and q disturbance coordinates. Runtime dimensions and model-supplied capabilities keep the engine independent of physical meaning. q=0 uses the unique empty disturbance vector and empty lo/hi bounds.

For each closed cell C, fixed selected input u and disturbance box W, return Y containing every f(x,u,w) for x in C and w in W. A centre-only simulation is insufficient.

For cooperative nondecreasing dynamics on the entire requested boxes, Y=[f(C.lo,u,W.lo), f(C.hi,u,W.hi)]. This needs analytic model justification, not a few samples.

For bounded derivatives, use centres xc/wc and radii rx/rw, c=f(xc,u,wc), r=Dx*rx+Dw*rw and Y=[c-r,c+r]. Dx and Dw are nonnegative componentwise absolute-Jacobian bounds over the whole boxes, shaped n×n and n×q. Matrices are row-major. With q=0, Dw has n empty rows. Signed negative derivatives cannot be used directly as radii.

The formulas may be sound in exact arithmetic while ordinary doubles remain UncertifiedFloat. A validated backend must enclose rounding errors throughout its computation; passing tests is not certification.

## State and transition conventions

Internal IDs are 1..M and OUT is 0. Input IDs are 0..A-1 and refer to physical vectors. Dimension 0 varies fastest. Point ownership is half-open except the final interval includes the domain's upper endpoint. Cell geometry uses closed boxes.

Regular successors are all cells whose closed boxes intersect Y. Add OUT when any Y.lo[d]<=domain.lo[d] or Y.hi[d]>=domain.hi[d], corresponding to closure of the complement. Thus outer-boundary contact can include OUT although the exact point is owned internally. OUT/action returns compact AllStates, meaning every ID 0..M.

Uniform-grid closed-intersection candidate bounds in exact arithmetic are first=max(0,ceil((Y.lo-L)/eta)-1), last=min(count-1,floor((Y.hi-L)/eta)) componentwise. Empty ranges have no internal hits. Avoid unsafe casts and handle floating boundary uncertainty conservatively.

## Small model-independent illustration

For illustration only, choose x_next=.5*x+u+w, X=[0,4] divided into four unit cells, and W=[-.1,.1]. From cell closure [1,2] and u=0, Y=[.4,1.1], intersecting regular cells 1 and 2. With u=1, Y=[1.4,2.1], intersecting cells 2 and 3. These are explanatory calculations, not new production code or new fixture files.

## Subsequent synthesis and later example

Part 2 receives a graph and separate safe/target/label specification. Robust controlled predecessors require an action whose every successor satisfies the condition; empty Explicit results must not succeed vacuously. Safety uses a controlled invariant fixed point. Reachability needs forced progress toward the target. A finite-automaton task can require product-state reasoning and finite policy memory. A single favourable path is insufficient under nondeterminism.

The later robot example must use this same engine, with model/configuration/region adapters outside generic algorithms. Its periodic heading needs sound interval/topology handling. The full paper task includes visiting one of two regions, then another, while avoiding a forbidden region; a simple safety run is not that complete task. Exact example support and its required contract extension are implementation work, not part of this documentation-only change.
