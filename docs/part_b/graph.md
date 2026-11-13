# Transition model and bundles

buildSymbolicModel borrows IStateGrid, IInputCatalog and IReachability views during construction. The resulting immutable graph owns every stored successor list; queries remain valid after the borrowed views disappear. Returned successor references are stable throughout the graph's lifetime.

The builder validates the grid specification/count, runtime dimensions, disturbance box, physical input vectors, finite positive-volume internal cell boxes, returned enclosures and sorted unique internal intersections. It asks the supplied grid for closed-box intersections. It adds OUT if an enclosure touches or crosses any outer domain boundary, following the existing closure convention. Empty complete-model rows and malformed views are errors. Geometry is never requested for OUT.

Rows are indexed by state*action_count+action. Every OUT/action row uses compact AllStates. Regular rows have sorted unique Explicit IDs, including OUT when appropriate. Statistics count all (M+1)*A state/action pairs, all explicitly stored successor IDs and elapsed abstraction time; universal rows add no explicit edges. The graph preserves the supplied backend's numeric guarantee.

Pair, ID, container-size and byte arithmetic is checked before graph storage allocation. max_state_input_pairs includes OUT rows; max_stored_edges counts explicit IDs only. max_estimated_bytes covers the persistent graph object, row-vector capacity and explicit-ID buffer capacities. It is an estimate of graph storage, not a process peak-memory limit: allocator bookkeeping, callback internals and temporary returned boxes/intersection vectors are outside this budget. The current interfaces cannot bound memory allocated inside user callbacks.

assembleModelBundle takes owned shared handles and metadata, then reuses the common validateBundle checks. It preserves provenance and rejects inconsistent/null handles or metadata. This consistency check is not exhaustive graph or numerical certification.

Eight independent cases use tiny two-dimensional table views and supplied enclosure callbacks, with no A or fixture link:

```sh
cmake --preset dev
cmake --build build/dev --target model_builder_tests --parallel 2
ctest --test-dir build/dev -R '^part_b\.model_builder\.' --output-on-failure --no-tests=error
```

They cover closure/OUT, multiple physical inputs, q=0, malformed source/enclosure/intersection data, pair/edge/byte budgets and arithmetic overflow, graph lifetimes/stable references and bundle consistency. Original thermostat regression tests additionally check the independent six-row oracle and counts of eight processed pairs/twelve stored edges.
