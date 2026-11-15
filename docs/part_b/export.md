# Model-package export

exportModelPackage writes the existing v1 manifest.json, cells.csv, inputs.csv and transitions.csv. It consumes public bundle views; production code links no fixture or A implementation. The exact format remains the shared contract in ../part1-to-part2.md and schemas/part1-model-v1.schema.json.

Preflight checks bundle metadata/handles, UTF-8 strings, dimensions, finite geometry/inputs, dimension-0-fastest index round trips, uniform-grid boundary consistency, complete graph rows and the transition-row budget. Explicit rows are nonempty, sorted, unique and in range; OUT rows are compact AllStates. A universal pair writes one CSV row rather than expanding every state. Invalid models and budget exhaustion leave an existing package unchanged and do not create a new destination.

Interior grid-boundary validation allows four representable-double spacings at domain scale to accommodate ordinary subtract/divide/multiply/add evaluation order. This is a format consistency check, not a change to grid ownership or a numerical guarantee; serialization preserves the supplied coordinates. Clear nonuniform, overlapping or mismatched cell bounds fail validation. All produced B backends/graphs remain UncertifiedFloat.

CSV output uses the classic locale, max_digits10 precision and LF with a final newline. Metadata, provenance and physical input vectors are preserved. JSON quotes, backslashes and control characters are escaped. The v1 input catalogue does not expose how it was constructed, so the manifest records explicit_caller_order for the enumeration supplied by that catalogue; it preserves action IDs and vectors without inventing Cartesian provenance.

For a single writer, new files are written and closed in a unique staging directory under the destination. Existing regular package files may be replaced; unrelated files are preserved. Reserved filenames that are symlinks, directories or other nonregular entries are rejected. The old manifest retires before replacement and the new manifest publishes after the CSV files. Publication errors attempt restoration; restoration failures explicitly report where backups remain. Stream/write/close/filesystem errors propagate.

Seven focused cases cover fixed and generic two-dimensional packages (including q=0 and regular universal rows), UTF-8/JSON escaping, numeric precision/locale, complete-row budgets, malformed bundles/views/geometry, invalid strings, replacement/preservation and filesystem/symlink errors:

```sh
cmake --preset dev
cmake --build build/dev --target model_export_tests --parallel 2
ctest --test-dir build/dev -R '^part_b\.model_export\.' --output-on-failure --no-tests=error
```

Tests use public test-only fixture/table views; this dependency does not enter symctrl_part_b.
