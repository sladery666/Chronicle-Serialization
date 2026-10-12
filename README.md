# Chronicle Lite

**Division of responsibilities**: I built this project using my C++ knowledge. Codex and I collaborated on the project architecture: I designed it and reviewed Codex’s suggestions. I wrote the implementation code, while Codex wrote the unit tests, which I reviewed. I subsequently completed three iterations of performance optimization and feature expansion. Codex evaluated the performance optimizations; the benchmark results are available in the project documentation.

A C++20 versioned binary save system with explicit type bindings, structural
migrations, and command-line inspection tools. An RPG scene demonstrates how
legacy saves evolve into the current Player V3 model while preserving their data.

The runtime builds independently of a game engine and has no third-party runtime
dependencies.

[Project overview](docs/PROJECT_OVERVIEW_EN.md) ·
[Workflow demo and screenshots](docs/DEMO_EN.md) ·
[Evolution log](docs/EVOLUTION.md)

## Highlights

- **Deterministic binary archives:** little-endian encoding, stable type and
  field IDs, and fields written in ID order.
- **Explicit C++ type bindings:** member declarations drive schemas, encoding,
  decoding, and validation. The external WorldSettings example extends the model
  without adding a custom core serializer.
- **Structural migration:** V1→V2 widens signed health and supplies a missing
  name; V2→V3 moves health into `Stats { health, maxHealth }`. Direct V1→V3 and
  staged migration produce identical bytes.
- **Safe parsing and file handling:** bounded reads, schema and value validation,
  temporary-file writes, and migration destinations that cannot overwrite an
  existing file.
- **Measured memory behavior:** borrowed parsing views, bulk writes, reserved
  capacity, and benchmarks reporting timing and allocation metrics.
- **Compatibility verification:** independently generated golden fixtures, unit
  tests, and real CLI integration tests.

## Quick Start

Requires **CMake 3.28+**, **Ninja**, and a **C++20 compiler**. First configuration
needs network access to fetch pinned tool/test dependencies: nlohmann/json 3.11.3
and GoogleTest 1.15.2, each verified by SHA-256.

Run from the project root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
```

Then save, reload, and inspect a V3 scene in the same shell session:

```sh
mkdir -p build/demo
export DEMO_DIR="$(mktemp -d build/demo/readme.XXXXXX)"
./build/debug/scene_demo save "$DEMO_DIR/scene_v3.bin"
./build/debug/scene_demo load "$DEMO_DIR/scene_v3.bin"
./build/debug/chronicle inspect "$DEMO_DIR/scene_v3.bin"
./build/debug/chronicle validate "$DEMO_DIR/scene_v3.bin"
./build/debug/chronicle to-json "$DEMO_DIR/scene_v3.bin"
```

The sample contains Meadow, player Ada, health 100/100, a position, and a
Sword/Potion inventory. JSON exposes nested `player.stats`.

## Migrate a Legacy Save

Continue in the same shell session:

```sh
./build/debug/scene_demo save "$DEMO_DIR/scene_v1.bin" 1
./build/debug/chronicle migrate "$DEMO_DIR/scene_v1.bin" "$DEMO_DIR/migrated_v3.bin"
./build/debug/scene_demo load "$DEMO_DIR/migrated_v3.bin"
./build/debug/chronicle diff "$DEMO_DIR/scene_v1.bin" "$DEMO_DIR/migrated_v3.bin"
```

The migration defaults to V3. V1 has no player name, so it becomes `Unknown`;
maximum health is inferred as `max(100, health)`. Negative health is preserved.
The source remains unchanged and the destination must not exist. Explicit
`--to-version 2` supports V1→V2; same-version migration and downgrades are rejected.
Diff returns **1** when it finds changes, including schema changes, and **0** for
equal scene data.

The file format and root Scene schema remain version 1. Player V3 uses a nested
Stats object; V1/V2 remain readable through the Scene convenience APIs.

## Extend the Model

The [external model example](examples/type_binding_demo/model.hpp) declares
WorldSettings and QuestState with `TypeBinding<T>` member bindings. Run:

```sh
./build/debug/type_binding_demo save "$DEMO_DIR/world.bin"
./build/debug/type_binding_demo load "$DEMO_DIR/world.bin"
./build/debug/type_binding_demo inspect "$DEMO_DIR/world.bin"
```

Generic APIs are `serializeBound`, `deserializeBound<T>`, `saveBound`, and
`loadBound<T>`. They support integers, finite floats, UTF-8 strings, bound objects,
and vectors of supported values. The ordinary `chronicle` CLI targets Scene;
the external-type example supplies its own bindings.

## Tools and Layout

| CLI command | Purpose |
| --- | --- |
| `inspect FILE` | Show types, schemas, fields, sizes, and offsets |
| `validate FILE` | Parse and validate a Scene archive |
| `diff OLD NEW` | Compare scene values and structural changes |
| `to-json FILE` | Export the current Scene model as JSON |
| `migrate SOURCE NEW [--to-version 2\|3]` | Upgrade into a new file |

| Path | Responsibility |
| --- | --- |
| `include/chronicle/`, `src/` | Public runtime APIs and implementations |
| `tools/cli/` | Inspection, validation, diff, JSON, and migration commands |
| `examples/` | Scene and external-type workflows |
| `tests/`, `tests/golden/` | Tests, independent fixture encoder, binary samples |
| `benchmarks/` | Timing and dynamic allocation instrumentation |
| `docs/` | Architecture, illustrated workflow, and evolution history |

## Validation and Benchmarks

The recorded local run on **2026-10-12** passed all **72 CTest tests** in each of
Debug, Release, and ASan/UBSan. GitHub Actions is configured for macOS and Ubuntu;
remote CI execution has not been verified. See the
[demo](docs/DEMO_EN.md) for screenshots and measurements.

Release and sanitizer builds use matching configure, build, and test presets:

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release
cmake --preset sanitizers
cmake --build --preset sanitizers --parallel 2
ctest --preset sanitizers
./build/release/chronicle_benchmark
./build/release/chronicle_benchmark --suite
```
