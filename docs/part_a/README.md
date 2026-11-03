# Part A: general discretisation and policy refinement

Branch part-a/discretization; edit only include/symctrl/part_a/, src/part_a/, tests/part_a/ and docs/part_a/. A's responsibilities and existing public signatures remain unchanged by the [scope clarification](../scope-correction.md).

Implement generic uniform grids, dimension-0-fastest IDs/indexing, cell bounds, exact point ownership, closed internal intersections, explicit/Cartesian input catalogues and refineDecision. Dimensions, physical domains and input vectors are supplied. The adapter consumes a supplied policy and memory; it does not synthesise a controller.

The factories remain stubs. A tests link A/common independently of B/fixtures. Read [contracts.md](../contracts.md), extend owned CMake/test/docs files, and preserve boundary/OUT/order/lifetime rules.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2
ctest --test-dir build/dev -L part_a --output-on-failure
python3 tools/check_readiness.py build/dev --mode a
python3 tools/check_ownership.py auto
```

Part 1 combines A with B's general transition model. Part 2 later supplies general synthesis; the robot example comes after the reusable pipeline. No implementation is added by this documentation-only revision.
