# Symbolic control

This C++17 project builds a finite model of a system with continuous states, computes control choices on that model, and turns symbolic choices back into physical inputs.

The physical system has the form `x_next = f(x, u, w)`: `x` is its state, `u` is the chosen input, and `w` is a bounded disturbance. A grid groups physical states into cells. For each cell and input, reachability analysis encloses all possible next states. Intersecting that enclosure with the grid produces a finite transition graph. A synthesis algorithm then chooses inputs that satisfy a separately supplied objective for **every** possible successor.

The engine supports runtime dimensions and supplied dynamics. Examples include the thermostat, circular building, and a mobile robot with periodic heading, mission synthesis, and an interactive dashboard.

Navigation: [implemented features](#what-is-implemented) · [build and run](#build-and-run) · [files](#where-the-code-lives) · [conventions](#data-and-conventions) · [function guide](#function-guide) · [library example](#use-the-library) · [export](#exported-model-package).

## What is implemented

| Area | Existing implementation |
| --- | --- |
| Discretization and refinement, paper Part 1 | Uniform grids, point-to-cell mapping, closed-cell intersections, explicit and Cartesian input catalogs, application of a supplied policy with memory. |
| Abstraction, paper Part 1 | Cooperative-monotone and bounded-derivative enclosures, transition construction and storage, storage budgets, immutable model bundles, JSON/CSV export. |
| Safety synthesis, paper Part 2 | Robust predecessor, greatest controlled invariant subset of a safe set, and all inputs that preserve that subset. |
| Reachability synthesis, paper Part 2 | Winning domain, worst-case progress ranks, and inputs whose successors all have smaller ranks. |
| Finite-automaton tasks, paper Part 2 | Deterministic automaton validation, product graph, accepting target, winning initial states, and a finite-memory policy. |
| Physical examples | Thermostat and circular-building dynamics, including their monotonicity checks and absolute derivative bounds. |
| Command-line application | Thermostat abstraction, a supplied-policy simulation, three-room building abstraction, and thermostat export. |
| Mobile robot and dashboard | Unicycle dynamics, periodic heading, finite-automaton missions, optimized synthesis, trajectory simulation, and a GLFW/Dear ImGui/ImPlot interface. |

All these sources are compiled by the default build. Synthesis is a C++ library API; the CLI commands demonstrate abstraction, refinement, and export. The safety and reachability controller builders return maps of allowed inputs. `AutomatonPolicy` implements the executable `ISymbolicPolicy` interface for automaton tasks.

Still absent: a general configuration-file reader, a model-package importer, a general cost/optimal-control synthesis API, and advanced large-model representations. There is no claimed full-paper application or scaling result.

Numerical backends currently report `UncertifiedFloat`. Their formulas depend on valid model assumptions, and floating-point calculations do not provide certified physical guarantees. Requesting validated reachability fails explicitly.

## Build and run

Requirements on all platforms: a C/C++ compiler with C++17 support, **CMake 3.21 or newer**, **Git** on `PATH`, a build tool, and graphics drivers supporting **OpenGL 3.0 or newer** on Windows/Ubuntu or **OpenGL 3.2** on macOS. Run the dashboard in a graphical desktop session.

During the first CMake configuration, Git downloads **GLFW 3.4**, **Dear ImGui v1.90.9**, and **ImPlot v0.16** automatically. Internet access is required for that step. Do not install these libraries separately; no Python, pip, or Node.js packages are needed. CMake also detects the system thread library automatically.

Run all build commands from the project root, the folder containing `CMakeLists.txt`. Use separate build directories for Windows, Ubuntu, and macOS.

### Ubuntu: install dependencies

On Ubuntu 22.04 or newer, install the compiler, Make, CMake, Git, OpenGL development files, and the X11/Wayland development dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config \
  libgl1-mesa-dev xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols
cmake --version
```

The X11 and Wayland packages follow the [GLFW build requirements](https://www.glfw.org/docs/3.4/compile.html). Both backends are enabled by default.

### Ubuntu: build and run the robot dashboard

```bash
cmake -S . -B build-ubuntu -DCMAKE_BUILD_TYPE=Release
cmake --build build-ubuntu --target robot_dashboard --parallel 2
./build-ubuntu/robot_dashboard
```

If startup aborts with `Settings schema 'org.gnome.settings-daemon.plugins.xsettings' does not contain a key named 'antialiasing'`, launch through X11:

```bash
XDG_SESSION_TYPE=x11 ./build-ubuntu/robot_dashboard
```

This selects GLFW's X11 backend and requires an X11/XWayland display. If the error persists in a VS Code Snap terminal, open Ubuntu Terminal with **Ctrl+Alt+T**, navigate to the project root, and run the same command there.

### Windows: install dependencies

Install these before building:

- [Git for Windows](https://git-scm.com/downloads/win), with Git available from the command line.
- [CMake 3.21 or newer](https://cmake.org/download/), selecting **Add CMake to the system PATH** during installation.
- **Visual Studio 2022** or **Build Tools for Visual Studio 2022**, with the **Desktop development with C++** workload. Include the **MSVC v143 x64/x86 build tools** and a **Windows 10 or Windows 11 SDK**. See [Microsoft's C++ installation instructions](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170).
- Your graphics vendor's Windows driver with OpenGL 3.0 support. The Windows SDK supplies the OpenGL development files; no separate GLFW, ImGui, ImPlot, or pthread package is required.

Restart the terminal after installation. Visual Studio Code alone does not supply a compiler or Windows SDK.

### Windows: build and run the robot dashboard

Open **PowerShell** in the project root. These commands use the [Visual Studio 2022 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html):

```powershell
git --version
cmake --version
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
cmake --build build-windows --config Release --target robot_dashboard --parallel 2
.\build-windows\Release\robot_dashboard.exe
```

For this Windows generator, `--config Release` selects the build configuration, and the executable is inside the `Release` subdirectory. On Ubuntu, `-DCMAKE_BUILD_TYPE=Release` selects it during configuration. After source changes, rerun the build command before launching again.

### macOS: install dependencies

Install Apple's Command Line Tools for Xcode, which provide Apple Clang, Make, Git, and the macOS SDK:

```bash
xcode-select --install
```

Wait for the installer to finish before continuing. If the tools are already installed, skip that step. Install [CMake with Homebrew](https://formulae.brew.sh/formula/cmake) if you have a [supported Homebrew installation](https://docs.brew.sh/Installation):

```bash
brew install cmake
cmake --version
git --version
```

Alternatively, install CMake from the [official macOS download](https://cmake.org/download/). If you place `CMake.app` in `/Applications`, make its command-line tools available in the current terminal with:

```bash
export PATH="/Applications/CMake.app/Contents/bin:$PATH"
cmake --version
```

The system SDK supplies OpenGL and the native frameworks GLFW needs. CMake downloads and builds GLFW, ImGui, and ImPlot automatically; no separate graphics library or XQuartz installation is needed.

### macOS: build and run the robot dashboard

Open Terminal in the project root on an Intel or Apple Silicon Mac:

```bash
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --target robot_dashboard --parallel 2
./build-macos/robot_dashboard
```

Use a native terminal and toolchain for your Mac's architecture; CMake selects the native architecture by default. Start with a fresh build directory when switching operating systems or architectures.

The dashboard selects an OpenGL 3.2 core, forward-compatible context with GLSL 150 on macOS, following the [Dear ImGui GLFW/OpenGL example](https://github.com/ocornut/imgui/blob/v1.90.9/examples/example_glfw_opengl3/main.cpp). macOS OpenGL deprecation warnings are silenced for the dashboard; the renderer still uses the system OpenGL framework.

### Command-line examples

On Ubuntu, also build and run the robot CLI or the original model examples:

```sh
cmake --build build-ubuntu --target robot_cli symctrl_cli --parallel 2
./build-ubuntu/robot_cli
./build-ubuntu/symctrl_cli --help
./build-ubuntu/symctrl_cli thermostat
./build-ubuntu/symctrl_cli thermostat-policy
./build-ubuntu/symctrl_cli building-n3
./build-ubuntu/symctrl_cli export-thermostat exports/thermostat
```

On Windows, build those targets with `cmake --build build-windows --config Release --target robot_cli symctrl_cli --parallel 2`, then use `.\build-windows\Release\robot_cli.exe` or `.\build-windows\Release\symctrl_cli.exe` with the same model arguments.

On macOS:

```bash
cmake --build build-macos --target robot_cli symctrl_cli --parallel 2
./build-macos/robot_cli
./build-macos/symctrl_cli thermostat
```

| Command | What it does |
| --- | --- |
| `--help` | Prints the available commands. |
| `thermostat` | Builds a graph over `[17,23]`, with three cells, inputs `{0,1}`, and disturbance `[10,15]`. Prints model sizes, numerical status, edge count, and elapsed abstraction time. |
| `thermostat-policy` | Builds that graph and simulates two physical steps from temperature `18`, using a manually supplied policy and disturbance `10`. This command does not synthesize its policy. |
| `building-n3` | Builds the three-room graph over `[17,22]^3`, using ten cells per dimension and input values `{0,0.3,0.6}` per room. There are 1,000 internal states and 27 actions. |
| `export-thermostat DIRECTORY` | Builds the thermostat model and writes its metadata, cells, inputs, and transitions to the directory. |

Exit codes are `0` for success, `1` for invalid usage or a reported exception, and `2` if a `NotImplemented` exception reaches the CLI. Build products and exports are generated files and are ignored by Git. There are no test sources, CTest targets, or readiness scripts in this cleaned project.

### Generated files and local settings

`.gitignore` excludes platform build directories (`build-ubuntu`, `build-windows`, `build-macos`), CMake output, compiled binaries, generated Xcode projects, local IDE settings, macOS/Windows metadata, `imgui.ini`, and the default simulation CSV exports. Your editor configuration and dashboard layout remain local; running the dashboard does not require a tracked `imgui.ini` file. Keep shared CMake presets in `CMakePresets.json`; personal overrides in `CMakeUserPresets.json` are ignored.

## Where the code lives

```text
readme.md                          This guide
CMakeLists.txt                     Builds libraries, CLIs, and the robot dashboard
app/symbolic_control.cpp           CLI commands and supplied example policy
app/robot_cli.cpp                  Robot synthesis and simulation CLI
app/robot_simulation.cpp           Optimized robot synthesis and closed-loop simulation
app/unicycle_dynamics.cpp          Mobile-robot dynamics
app/mission_compiler.cpp           Region labels and mission automata
app/dashboard/                    Interactive robot dashboard
include/symctrl/                   Public types, interfaces, and factories
include/symctrl/synthesis/         Public synthesis and automaton APIs
src/common/                       Value, successor-set, and bundle validation
src/discretization/               Uniform grid, inputs, runtime refinement
src/abstraction/                  Enclosures, transition graph, model export
src/models/                       Thermostat and circular-building dynamics
src/synthesis/                    Safety, reachability, automata, product model
```

Each source directory has a short `CMakeLists.txt` that lists its compilation units. Internal helper headers live beside their implementations. Public headers require only other public headers.

The old ownership names `part_a`, `part_b`, `part2`, and `part2_b` have been replaced by folders describing their purpose. Public include paths now start directly at `symctrl/`, for example `symctrl/factories.hpp`. General types and Part 1 functions use namespace `symctrl`; synthesis keeps the existing namespace `symctrl::part2`.

Link the CMake target `symctrl::symctrl` to use the complete library. Individual targets are `symctrl_common`, `symctrl_discretization`, `symctrl_abstraction`, `symctrl_models`, and `symctrl_synthesis`.

## Data and conventions

| Name | Meaning |
| --- | --- |
| `Vector` / `Matrix` | Dynamic arrays of doubles; matrices are indexed `[row][column]`. |
| `Box {lo, hi}` | Closed coordinate bounds: each coordinate lies between its lower and upper values. |
| `GridSpec` | Physical domain and the number of cells per coordinate. |
| `StateId` / `InputId` | Unsigned 64-bit state/action identifiers. |
| `kOutside` or OUT | State `0`, representing points outside the modeled domain. It has no finite cell bounds. |
| Internal states | IDs `1..M`, where `M` is the product of all grid counts. Coordinate 0 varies fastest. |
| Actions | IDs `0..A-1`; each names one physical input vector. Every action is available at every state of the current complete models. |
| `DerivativeBounds {Dx, Dw}` | Nonnegative absolute derivative bounds over the entire state/disturbance boxes. Shapes are `n × n` and `n × q`. |
| `SuccessorSet` | Either sorted, unique explicit state IDs, or compact `AllStates`. |
| `AllStates` | Its ID vector is empty, but it means **every** state `0..M`. An empty `Explicit` set means no successors. |
| `BuildOptions` | Maximum state/action pairs, stored edges, and estimated graph bytes. Defaults: 1,000,000 pairs, 10,000,000 edges, 512 MiB. |
| `BuildStatistics` | Processed pairs, stored explicit edges, and construction time in seconds. OUT rows count as pairs; compact universal rows store no explicit edges. |
| `ModelMetadata` / `ModelBundle` | Versions, model identity, parameters, geometry, disturbance, numerical status, and shared immutable grid/input/graph handles. |
| `PolicyMemory` / `PolicyDecision` | Previous memory, then the updated memory and selected action. |
| `ConcreteDecision` | Located state, action, an owned physical input vector, and updated memory. |
| `StateSet` | A set of state IDs used for safe, target, and winning regions. |
| `SafetyController` / `ReachabilityController` | Maps from states to allowed action lists. |
| `ReachabilityResult` | Winning domain plus progress rank per state; targets have rank 0. |
| `Automaton` / `StateLabels` | Initial automaton state, final states, transition table, and one label for every internal system state. |
| `ProductState` | A pair of system state and automaton state. |

For point ownership, cells are half-open, except that the last cell includes the domain's upper endpoint. Cell geometry and enclosure intersections are closed, so a point on an internal face belongs to one cell but can intersect both neighboring cell closures.

An enclosure touching or crossing an outer domain boundary adds OUT as a successor. Every OUT/action row has `AllStates` successors. An empty explicit successor set cannot make a synthesis choice succeed vacuously.

State/input dimensions are positive. Disturbance dimension can be zero: use empty disturbance vectors and `Box{{}, {}}`. Shapes and coordinates must be valid and finite. Public validators reject inconsistent shapes and checked arithmetic rejects count overflow. The grid's existing floating-point indexing is not validated interval arithmetic; extremely small cell widths and extreme coordinate ratios require care.

Shared handles keep objects alive. Grid/input factories own their data, enclosure objects retain the supplied dynamics, and constructed graphs own their rows. A returned input reference remains valid for the catalog lifetime; `refineDecision` copies it into the result.

## Function guide

The following tables cover public functions, interface methods, and the internal helpers used by the implementations. Constructors establish owned data; default interface destructors release objects through their base pointers.

### Value checks and successor sets

Declarations: [types.hpp](include/symctrl/types.hpp), [dynamics.hpp](include/symctrl/dynamics.hpp), [transition_system.hpp](include/symctrl/transition_system.hpp), and [model_bundle.hpp](include/symctrl/model_bundle.hpp). Implementations: [value_validation.cpp](src/common/value_validation.cpp), [successor_sets.cpp](src/common/successor_sets.cpp), and [model_bundle_validation.cpp](src/common/model_bundle_validation.cpp).

| Function | What it does |
| --- | --- |
| `checkedAdd(a, b)` | Adds unsigned counts and throws on overflow. |
| `checkedMultiply(a, b)` | Multiplies unsigned counts and throws on overflow. |
| `checkedSize(value)` | Converts a count to `size_t` after checking that it fits. |
| `validateVector(value, dimension)` | Requires the exact dimension and finite coordinates. |
| `validateBox(box, allow_empty_disturbance)` | Checks matching finite bounds and `lo <= hi`. Empty bounds are allowed only when the flag is true. |
| `validateGridSpec(spec)` | Checks positive domain widths, positive cell counts, consistent dimensions, and representable counts. Returns `M`. |
| `validateMatrix(value, rows, columns)` | Checks shape and finite, nonnegative entries. It is a validator for absolute derivative-bound matrices. |
| `validateDerivativeBounds(bounds, n, q)` | Requires a positive state dimension and correctly shaped `Dx`/`Dw`. |
| `toString(NumericGuarantee)` | Returns `uncertified_float` or `validated_enclosure`; rejects unknown enum values. |
| `toString(Provenance)` | Returns `hand_authored_fixture` or `generated_part1`; rejects unknown enum values. The first value remains part of the metadata vocabulary. |
| `validateSuccessors(set, M)` | Checks the representation: explicit IDs must be sorted, unique, and within `0..M`; `AllStates` must store no IDs. |
| `contains(set, state, M)` | Checks membership, including universal membership for `AllStates`. Rejects out-of-range queries. |
| `nonempty(set, M)` | Distinguishes empty explicit rows from nonempty explicit or universal rows. |
| `forEachSuccessor(set, M, visit)` | Calls the visitor once for every successor, expanding `AllStates` over `0..M`. |
| `validateBundle(bundle)` | Checks nonnull handles, versions, metadata, dimensions, counts, grid agreement, disturbance, parameters, and numerical status. This is consistency checking, not exhaustive graph certification. |
| `NotImplemented(feature)` | Constructs an exception naming an unavailable feature. `UnsupportedCapability` reports unavailable mathematical or numerical capabilities. |

### Grid, inputs, and runtime refinement

Public entry points: [factories.hpp](include/symctrl/factories.hpp). Interfaces: [state_grid.hpp](include/symctrl/state_grid.hpp), [input_catalog.hpp](include/symctrl/input_catalog.hpp), and [policy.hpp](include/symctrl/policy.hpp).

| Function or method | What it does |
| --- | --- |
| `makeUniformGrid(spec)` | Constructs the private `UniformGrid` implementation, validates the grid, and computes strides and cell widths. |
| `IStateGrid::stateDimension()` | Returns the number of state coordinates. |
| `IStateGrid::spec()` | Returns the domain and cell counts. |
| `IStateGrid::internalStateCount()` | Returns `M`, excluding OUT. |
| `IStateGrid::locate(x)` | Maps a finite physical point to its owning cell, or OUT when outside the domain. |
| `IStateGrid::cellBounds(state)` | Returns the closed box of an internal cell. Rejects OUT and invalid IDs. |
| `IStateGrid::indices(state)` | Decodes an internal ID into zero-based coordinate indices. |
| `IStateGrid::fromIndices(index)` | Encodes coordinate indices as `1 + sum(index[d] * stride[d])`. |
| `IStateGrid::intersectingInternalCells(box)` | Enumerates sorted unique internal cells intersecting a closed box. Does not add OUT. |
| `makeInputCatalog(values)` | Constructs `ExplicitInputCatalog`, preserving the caller's order and allowing duplicate physical inputs as distinct actions. |
| `makeCartesianInputs(axes)` | Constructs `CartesianInputCatalog` and precomputes every combination of axis values, with coordinate 0 varying fastest. |
| `IInputCatalog::inputDimension()` | Returns coordinates per input vector. |
| `IInputCatalog::inputCount()` | Returns the number of actions. |
| `IInputCatalog::value(action)` | Returns the stored physical vector; rejects invalid actions. |
| `ISymbolicPolicy::decide(previous_memory, state)` | Updates memory from the current symbolic state and returns an action. Missing entries or invalid memory are errors. This is an interface for a supplied or synthesized policy. |
| `refineDecision(grid, inputs, policy, measured_x, memory)` | Locates the measurement, rejects OUT, calls the policy, checks the action, and returns its physical vector plus updated memory. |

Implementations: [uniform_grid.cpp](src/discretization/uniform_grid.cpp), [input_catalog.cpp](src/discretization/input_catalog.cpp), and [policy_refinement.cpp](src/discretization/policy_refinement.cpp).

### Dynamics and reachable enclosures

Interfaces: [dynamics.hpp](include/symctrl/dynamics.hpp) and [reachable_enclosure.hpp](include/symctrl/reachable_enclosure.hpp). A new physical model implements `IDynamics`; it does not require changing the general grid, abstraction, or synthesis algorithms.

| Function or method | What it does |
| --- | --- |
| `IDynamics::stateDimension()` | Returns the number of physical state coordinates, `n`. |
| `IDynamics::inputDimension()` | Returns input dimension, `m`. |
| `IDynamics::disturbanceDimension()` | Returns disturbance dimension, `q`, possibly zero. |
| `IDynamics::step(x, u, w)` | Computes one physical update. |
| `IDynamics::cooperativeOn(states, input, disturbances)` | Reports analytically justified nondecreasing dependence on state and disturbance throughout the requested boxes at the fixed input. |
| `IDynamics::absoluteJacobianBounds(states, input, disturbances)` | Returns full-box absolute derivative bounds. Unsupported models must report failure rather than use a center-only Jacobian. |
| `makeThermostatDynamics()` | Constructs the scalar thermostat model described below. |
| `makeRingBuildingDynamics(rooms)` | Constructs a circular-building model; requires at least three rooms and representable dimensions. |
| `makeReachability(dynamics, method, options)` | Validates the supplied model and selects `CooperativeMonotone` or `BoundedDerivative`. Rejects requests for validated arithmetic. |
| `IReachability::stateDimension()` / `inputDimension()` / `disturbanceDimension()` | Return the dimensions of the retained dynamics. |
| `IReachability::numericGuarantee()` | Reports the backend's numerical status; both existing implementations return `UncertifiedFloat`. |
| `IReachability::enclose(cell, input, disturbance_box)` | Encloses one-step outcomes for the entire cell and disturbance box, using the selected method. |

The [monotone backend](src/abstraction/monotone_reachability.cpp) first checks `cooperativeOn`, then evaluates the lower and upper corners. The enclosure is `[f(cell.lo,u,W.lo), f(cell.hi,u,W.hi)]`.

The [derivative backend](src/abstraction/derivative_reachability.cpp) evaluates the centers `xc`, `wc`, then calculates radius `r = Dx * rx + Dw * rw`. Its enclosure is `[f(xc,u,wc)-r, f(xc,u,wc)+r]`. Bounds must hold over the full boxes.

The [thermostat](src/models/thermostat_dynamics.cpp) uses `x_next = (1-a-b*u)*x + a*w + b*T*u`, with `a=0.1`, `b=0.2`, and `T=30`. It is cooperative when `1-a-b*u >= 0`; its derivative bounds are `|1-a-b*u|` and `a`.

The [building](src/models/ring_building_dynamics.cpp) uses circular neighbors and `x_next[i] = (1-2*alpha-beta-gamma*u[i])*x[i] + alpha*x[previous] + alpha*x[next] + beta*Te + gamma*Th*u[i]`. Constants are `alpha=0.45`, `beta=0.045`, `gamma=0.09`, `Te=-1`, and `Th=50`. It has one input per room and no disturbances. Its monotonicity check requires nonnegative diagonal coefficients; its derivative matrix has absolute diagonal coefficients and neighbor coefficients `alpha`.

Internal helpers:

| Helper | Purpose |
| --- | --- |
| `detail::validateDynamicsBox(...)` | Checks a model's state, input, and disturbance request. Defined in [dynamics_validation.hpp](src/models/dynamics_validation.hpp). |
| `detail::checkedDynamicsValue(value)` | Converts a long-double result to finite double or throws on overflow. |
| `diagonal(input)` | Calculates the building's self-coupling coefficient. |
| `detail::makeMonotoneReachability(dynamics)` / `makeDerivativeReachability(dynamics)` | Construct the private enclosure classes after the public factory has checked the request. |
| `finiteResult(value)` | Rejects nonfinite intermediate derivative-enclosure arithmetic. |
| `centreRadius(box)` | Computes centers and covering radii while avoiding avoidable midpoint overflow. |
| `addRadius(radius, bounds, halfWidths)` | Adds checked derivative-bound contributions to one output radius. |

### Transition graph and model export

| Function or method | What it does |
| --- | --- |
| `buildSymbolicModel(grid, inputs, reachability, disturbance, options)` | Validates compatible views, constructs all cell/action rows, adds every enclosure intersection and OUT where needed, and enforces graph budgets. The returned graph owns its data. |
| `ITransitionSystem::internalStateCount()` | Returns the internal state count. |
| `ITransitionSystem::inputCount()` | Returns the action count. |
| `ITransitionSystem::numericGuarantee()` | Returns the status inherited from the enclosure backend or original product model. |
| `ITransitionSystem::statistics()` | Returns construction counts and duration. |
| `ITransitionSystem::successors(state, action)` | Returns the stored successor row, including OUT rows; rejects invalid IDs. |
| `assembleModelBundle(grid, inputs, transitions, metadata)` | Retains the immutable views, attaches the supplied metadata, calls `validateBundle`, and returns the bundle. |
| `exportModelPackage(bundle, directory, options)` | Checks the bundle and export-row limit, writes staged files, then publishes the JSON/CSV package. Existing package files are replaced with rollback on publication failure. |
| `detail::explicitTransitionStorageBytes(pairs)` | Estimates the persistent graph object and row-header storage, before explicit edge buffers. |
| `detail::makeExplicitTransitionSystem(...)` | Constructs the private `ExplicitTransitionSystem` with owned rows and statistics. |

Graph implementations: [model_builder.cpp](src/abstraction/model_builder.cpp) and [transition_storage.cpp](src/abstraction/transition_storage.cpp). Row lookup uses `state * input_count + action`. Row headers cover all state/action pairs; explicit successor IDs are stored only for rows that need them.

Private helpers in [model_export.cpp](src/abstraction/model_export.cpp):

| Helper | Purpose |
| --- | --- |
| `validateUtf8(text)` | Rejects malformed UTF-8 metadata. |
| `quoted(stream, text)` | Writes an escaped JSON string. |
| `array(stream, values)` / `box(stream, value)` | Write JSON arrays and bound objects. |
| `manifest(stream, metadata)` | Writes the versioned metadata document and file references. |
| `validateCell(grid, id, metadata)` | Checks index round trips, ordering, and agreement between cell bounds and uniform-grid metadata. |
| `preflight(bundle, options)` | Checks metadata, geometry, inputs, complete graph rows, universal OUT rows, and the export budget before writing. |
| `writeFile(path, writer)` | Writes a file using the classic locale and round-trip double precision; reports write/flush failures. |
| `StagingDirectory(parent)` / `~StagingDirectory()` | Create a unique temporary export directory and clean it up unless rollback backups must be retained. |
| `publish(staging, directory)` | Backs up old package files, installs the new CSVs and then the manifest, and restores old files if installation fails. |

`ExportOptions::max_transition_rows` defaults to 10,000,000. This counts emitted CSV rows: each explicit edge occupies a row, while a universal successor set occupies one row.

### Robust predecessor and safety

These functions use namespace `symctrl::part2`. The specification is supplied as state sets, independently of the physical model.

| Function | What it does |
| --- | --- |
| `detail::keepsInside(model, state, input, region)` | Returns true only if the successor row is nonempty and every successor belongs to `region`. Shared internal helper in [successor_condition.hpp](src/synthesis/successor_condition.hpp). |
| `robustPredecessor(model, region)` | Returns internal states having at least one input that keeps every possible successor in `region`. See [robust_predecessor.cpp](src/synthesis/robust_predecessor.cpp). |
| `computeSafetyDomain(model, safeStates)` | Repeatedly replaces the candidate by its intersection with its robust predecessor until unchanged. Returns the largest controlled invariant subset of the safe set. See [safety_controller.cpp](src/synthesis/safety_controller.cpp). |
| `buildSafetyController(model, safetyDomain)` | Lists all actions that keep each domain state inside the domain. Pass the fixed point from `computeSafetyDomain`; passing an arbitrary region can produce empty action lists. |

Supply valid internal IDs for safe sets. OUT is excluded from winning states by the predecessor enumeration. The algorithms use universal quantification over successors, so a single favorable path does not establish safety.

### Reachability and progress

Declarations: [reachability_controller.hpp](include/symctrl/synthesis/reachability_controller.hpp). Implementation: [reachability_controller.cpp](src/synthesis/reachability_controller.cpp).

| Function | What it does |
| --- | --- |
| `computeReachabilityDomain(model, targetStates)` | Validates internal target IDs, starts targets at rank 0, and repeatedly adds robust predecessors. Newly added states receive the iteration's rank. Stops at the winning fixed point. |
| `buildReachabilityController(model, result)` | Checks the domain/rank mapping and lists actions whose successors all have strictly smaller ranks. Throws if a positive-rank winning state has no progress action. Target states have empty action lists because the goal is already reached. |

The rank bounds the number of symbolic steps needed to force a target visit when the returned progress choices are used. This API specifies eventual reachability; extra avoidance requirements must be represented separately, for example in an automaton/product specification.

### Finite automata and product models

Declarations: [finite_automaton.hpp](include/symctrl/synthesis/finite_automaton.hpp), [product_model.hpp](include/symctrl/synthesis/product_model.hpp), and [automaton_policy.hpp](include/symctrl/synthesis/automaton_policy.hpp). Implementations have the same filenames under `src/synthesis/`.

An automaton state records progress through a task. Its transition table is indexed by `[automaton_state][label]`; entries and labels start at zero. Every internal physical cell must have one label. The product graph records both the system cell and task progress.

| Function or method | What it does |
| --- | --- |
| `validateAutomaton(automaton)` | Checks a nonempty rectangular transition table, valid initial/final states, at least one label, and valid transition destinations. |
| `nextAutomatonState(automaton, state, label)` | Validates the automaton and indices, then returns the transition table entry. |
| `productStateCount(M, Q)` | Returns checked product size `M * Q`; both factors must be positive. |
| `encodeProductState(state, q, M, Q)` | Maps an internal system state and automaton state to `state + M*q`. |
| `decodeProductState(id, M, Q)` | Reverses that mapping. OUT has no system/automaton pair. |
| `buildProductModel(model, automaton, labels, options)` | Builds an owned transition graph over pairs. Each successor's label updates automaton memory. System OUT maps to product OUT; product OUT has universal successors. Enforces storage budgets. |
| `buildProductTarget(M, automaton)` | Returns every product pair whose automaton state is final. OUT is excluded. |
| `computeWinningInitialStates(M, automaton, labels, winningDomain)` | Reads each initial system label from the automaton's initial state and returns system states whose resulting pair belongs to the winning domain. |
| `AutomatonPolicy(M, A, automaton, labels, controller)` | Owns and validates the automaton, labels, product-controller entries, and sorted unique action lists. It consumes a controller computed on the product; it does not synthesize one in its constructor. |
| `AutomatonPolicy::initialMemory()` | Returns the automaton's initial state as policy memory. |
| `AutomatonPolicy::isFinished(previous_memory, current_state)` | Reads the current label and checks whether the resulting automaton state is final. Does not mutate stored memory. |
| `AutomatonPolicy::decide(previous_memory, current_state)` | Reads the current label, finds the product controller entry, and selects its first allowed input. Returns the new automaton memory. Throws if the goal is already reached or no applicable entry exists. |
| `AutomatonPolicy::nextState(memory, state)` | Private helper validating the measured state and previous memory, then applying its label transition. |
| `validateLabels(labels, M, labelCount)` | Private product-builder helper requiring exactly one valid label for every internal system state. |

`ProductTransitionSystem` owns its rows and implements the same graph query methods documented above. `AutomatonPolicy` validates controller structure; its guarantees depend on using a controller actually synthesized for the matching graph and specification.

To use an automaton task:

1. Supply the automaton and one conservative label per system cell.
2. Call `buildProductModel` and `buildProductTarget`.
3. Call `computeReachabilityDomain` on the product and its target.
4. Call `buildReachabilityController` with that result.
5. Use `computeWinningInitialStates` to check admissible starts.
6. Construct `AutomatonPolicy` and initialize memory with `initialMemory()`.
7. At each measurement, call `isFinished` first. Otherwise use `refineDecision` with the same previous memory, apply the returned physical input, and retain `next_memory`.

### Command-line entry point

In [symbolic_control.cpp](app/symbolic_control.cpp), `main(argc, argv)` parses the four model commands, constructs the example pipeline, prints statistics, optionally exports the bundle, and reports exceptions. The local `SuppliedPolicy::decide(memory, state)` accepts thermostat states `1..3`, selects action `1` in state `1` and action `0` otherwise, and leaves memory unchanged. It is a demonstration table.

## Use the library

This example builds a thermostat abstraction, computes its safety domain, and computes inputs that force a visit to cell 2:

```cpp
#include "symctrl/factories.hpp"
#include "symctrl/synthesis/safety_controller.hpp"
#include "symctrl/synthesis/reachability_controller.hpp"
#include <iostream>

int main() {
    using namespace symctrl;
    using namespace symctrl::part2;

    auto grid = makeUniformGrid(GridSpec{{{17}, {23}}, {3}});
    auto inputs = makeInputCatalog({{0}, {1}});
    auto dynamics = makeThermostatDynamics();
    auto enclosure = makeReachability(
        dynamics, ReachabilityMethod::CooperativeMonotone);
    auto model = buildSymbolicModel(
        *grid, *inputs, *enclosure, Box{{10}, {15}});

    const auto safe = computeSafetyDomain(*model, StateSet{1, 2, 3});
    const auto safety = buildSafetyController(*model, safe);
    const auto goal = computeReachabilityDomain(*model, StateSet{2});
    const auto progress = buildReachabilityController(*model, goal);

    std::cout << "Safe states: " << safe.size() << '\n';
    for (const auto& entry : safety) {
        std::cout << "state " << entry.first << " safe inputs:";
        for (InputId input : entry.second) std::cout << ' ' << input;
        std::cout << '\n';
    }
    std::cout << "States reaching cell 2: " << progress.size() << '\n';
}
```

For an application in a separate CMake project, save that code as `main.cpp` and link the library:

```cmake
cmake_minimum_required(VERSION 3.21)
project(ControlApplication LANGUAGES CXX)
add_subdirectory("/absolute/path/to/Automate Project" symctrl-build)
add_executable(control_application main.cpp)
target_link_libraries(control_application PRIVATE symctrl::symctrl)
```

To apply a safety/reachability map through `refineDecision`, supply an `ISymbolicPolicy` implementation that chooses an allowed action and handles terminal/missing states. `AutomatonPolicy` already provides this interface for product reachability. Stop when its `isFinished` method reports success.

## Exported model package

| File | Contents |
| --- | --- |
| `manifest.json` | Contract/schema versions `1.0.0`, model identity and parameters, provenance, dimensions, domain/grid, disturbance, numerical status, ordering semantics, and CSV filenames. |
| `cells.csv` | `state_id`, then all `lower_d` columns and all `upper_d` columns. Contains internal cells only. |
| `inputs.csv` | `input_id`, followed by physical coordinates `u_0`, `u_1`, etc. |
| `transitions.csv` | `state_id,input_id,kind,successor_id`. Explicit rows use `explicit` and one successor ID per row. Universal rows use `all` with an empty final field. |

The exported model contains the dynamics abstraction, not a safety/target/automaton specification or a synthesized policy. CSV readers must preserve universal-row semantics. The library currently exports this format but does not import it.

## Mathematical sources and remaining work

The general stages follow Girard, Meyer, and Saoud, *Approches symboliques pour le contrôle des systèmes non linéaires*, S7467 V1 (2024): Section 1 supplies abstraction/refinement, Section 2 supplies synthesis, and Section 3 supplies the later robot example. The thermal models and circular-building challenge also appear in Saoud's *Project – Symbolic Control*, Computation theory, Fall 2026.

Development A/B were ownership tracks within Part 1. They do not indicate different paper stages. The current folders express the mathematical responsibilities directly. The robot application implements periodic heading and region labels; general cost optimization and scalable representations remain separate extensions of the reusable pipeline.

test latif 2