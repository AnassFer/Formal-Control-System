# Sources, scope and independent oracle

Authority: the supplied S7467 V1 article defines abstraction mathematics; the Fall 2026 slides define the ring-building parameters. The optional guide was absent. The reference Python implementation is secondary and was not used to implement this scaffold. Original local filenames and reading instructions are in `reference/README.md`.

| Source | Mapping to future code |
|---|---|
| Paper §1.1.1, printed 3 / PDF 4 | A: state partition, q(x), finite physical input selection and p(action) |
| Paper §1.1.2, printed 4 / PDF 5 | B: enclosure of every closed-cell state/disturbance and closure intersections |
| Paper §1.1.3, printed 5 / PDF 6 | B: cooperative endpoints and bounded absolute derivatives |
| Paper §1.2, printed 6 / PDF 7 | Shared policy interface; A runtime memory update, action choice and refinement |
| Paper §2, printed 7 / PDF 8 onward | Outside Part 1; controller synthesis is reserved for Part 2 |
| Slides 14–16 (numbered) | Fixed building dynamics/discretisation and later full safety competition |

The slides prescribe no incompatible A/B division. Our A/B grouping is development ownership and preserves the article's Section 1 boundary. The slides' thermostat objective `[17,21]` conflicts with their three displayed cells extending to 23, and a forbidden-action annotation is inconsistent. The article's `[17,23]` thermostat and the independent table below govern this repository. The building has its own `[17,22]^N` domain.

## Mathematical contract (future B implementation)

For each internal state cell C and physical input u, enclose `{f(x,u,w): x in closure(C), w in W}`. Cooperative nondecreasing dynamics give `[f(C.lo,u,W.lo), f(C.hi,u,W.hi)]` when justified on the entire requested boxes. With no disturbances the unique w is the empty vector.

The derivative alternative uses centres `xc,wc`, radii `rx,rw`, `c=f(xc,u,wc)`, `r=Dx*rx+Dw*rw`, and `[c-r,c+r]`. `Dx` and `Dw` are nonnegative componentwise absolute-Jacobian upper bounds over C×W, with shapes n×n and n×q. Matrices are row-major. A centre Jacobian, signed negative radius or a few monotonicity samples is insufficient.

Internal successors are every cell whose **closed** bounds intersect the enclosure Y. OUT is included if any `Y.lo[d] <= domain.lo[d]` or `Y.hi[d] >= domain.hi[d]`: this is intersection with the closure of the domain complement. Touching an outer boundary counts OUT although point abstraction owns that endpoint internally. Touching an internal face counts both neighbouring cell closures. For OUT/action, use compact `AllStates`, meaning all IDs `0..M`.

For a future uniform-grid intersection, exact-arithmetic candidate index bounds are `first=max(0,ceil((Y.lo-L)/eta)-1)` and `last=min(cells-1,floor((Y.hi-L)/eta))` componentwise. Empty ranges have no internal hits. Avoid unchecked casts of negative/huge indices; handle floating index-boundary error conservatively. No production enumeration is implemented now.

The mathematics can be sound in exact arithmetic while a double implementation remains uncertified. A future validated backend must enclose rounding errors in every operation. Testing sampled trajectories does not prove universal soundness or certification.

## Thermostat oracle

`f(x,u,w)=(0.9-0.2*u)*x+0.1*w+6*u`; X=[17,23], W=[10,15], physical inputs [0] and [1] in that ID order. The x derivative is 0.9 when OFF and 0.7 when ON; w derivative is 0.1, so both are nonnegative throughout all boxes. State closures are [17,19], [19,21], [21,23].

| State | Action / physical u | Exact interval | Successors |
|---|---|---|---|
| 1 | 0 / 0 | [16.3,18.6] | {0,1} |
| 1 | 1 / 1 | [18.9,20.8] | {1,2} |
| 2 | 0 / 0 | [18.1,20.4] | {1,2} |
| 2 | 1 / 1 | [20.3,22.2] | {2,3} |
| 3 | 0 / 0 | [19.9,22.2] | {2,3} |
| 3 | 1 / 1 | [21.7,23.6] | {0,3} |

Both OUT rows are universal. Hand-supplied memoryless policy: 1→ON, 2→OFF, 3→OFF. This is an explicit demonstration table, never a synthesis result. Independently calculated steps are `18,ON,w=10 -> 19.60` (state 2) and `19.60,OFF,w=10 -> 18.64` (state 1). Frozen integration tests compare both methods and graph edges with this table, then check bounded deterministic disturbance sequences. No algorithm was used to generate the fixtures.

## Circular building (future application only)

For N>=3, periodic neighbours, `Ti_next=Ti+0.45*(Tnext+Tprev-2*Ti)+0.045*(-1-Ti)+0.09*(50-Ti)*ui`. There is no variable disturbance. Use empty Box/vectors for q=0. The fixed benchmark uses `[17,22]^N`, ten cells per dimension (width .5), input axes `{0,.3,.6}` and dimension 0 fastest.

For a fixed input the self coefficient is `.055-.09*ui`, at least .001 for selected ui; both neighbour coefficients are .45 and every other state coefficient is zero. Thus the model is cooperative on the benchmark domain, and its constant nonnegative coefficients are also absolute derivative bounds. `Dw` has N rows of zero columns. The zero-based code neighbours of i are `(i+N-1)%N` and `(i+1)%N`; N>=3 prevents coincident neighbours.

Independent N=3 check: `[17,18,19]`, input `[0,.3,.6]` gives `[17.54,18.009,18.424]`. Uniform 20°C with all OFF gives `[19.055,19.055,19.055]`; all .6 gives `[20.675,20.675,20.675]`. For N=4, `[17,18,19,20]` and all OFF gives `[17.99,17.145,18.1,17.255]`. These literal checks are scaffolded in B tests, not a production model implementation.

| N | M=10^N (excluding OUT) | A=3^N | Internal cell/action pairs |
|---|---|---|---|
| 3 | 1,000 | 27 | 27,000 |
| 4 | 10,000 | 81 | 810,000 |
| 5 | 100,000 | 243 | 24,300,000 |
| 6 | 1,000,000 | 729 | 729,000,000 |

No benchmark is run in bootstrap. Future official runs retain the fixed constants/grid/input levels; coarse test grids must be labelled separately. Report configuration, numerical status, internal states, input count, processed pairs (including OUT), stored explicit edges, abstraction wall time and measured peak RSS where available. The CLI explicitly says RSS unavailable when it is not measured. Factory graph construction timing excludes export and prior grid/input creation; full process RSS measured externally includes the whole process. Do not invent timings, a largest certified N, synthesis time or controllable-state percentages at this milestone.
