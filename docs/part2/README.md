# Reserved base for general Part 2 synthesis

Part 2 consumes the generic transition model from Part 1 plus a separate specification and returns a symbolic policy. It must remain independent of physical model names and constants. The current subtree is a base only: no synthesis algorithm is implemented.

Owned paths are include/symctrl/part2/, src/part2/, tests/part2/ and docs/part2/. SYMCTRL_BUILD_PART2 enables the existing INTERFACE workspace target/local CMake entry points. Add compiled sources and algorithm tests there when implementation is authorised.

Read [PART2_HANDOFF.md](../../PART2_HANDOFF.md), [the quickstart](../part2-quickstart.md), [contracts](../contracts.md) and [the model/policy handoff](../part1-to-part2.md). Fixtures can be consumed now without A/B factories. Agree exact specification/result/factory types before public synthesis APIs are added. Later the completed general pipeline will be demonstrated with a mobile robot.
