# Chronicle Lite: Project Overview

Chronicle Lite is a C++20 versioned binary save system. The project uses an RPG scene to demonstrate deterministic serialization, safe parsing, explicit type bindings, and structural migration of legacy saves. The standalone runtime and CLI build and run independently of a game engine. The current model is Player V3; the file format remains version 1.

## Project Structure

```text
Epic/
├── CMakeLists.txt / CMakePresets.json
├── .clang-format / cmake/CompilerWarnings.cmake
├── include/chronicle/
│   ├── result.hpp / byte_buffer.hpp
│   ├── binary_reader.hpp / binary_writer.hpp
│   ├── schema.hpp / archive.hpp
│   ├── type_binding.hpp / bound_serialization.hpp
│   ├── scene.hpp / scene_bindings.hpp
│   └── serialization.hpp / migration.hpp
├── src/                         Runtime implementations
├── tools/cli/                   Inspect / Validate / Diff / JSON / Migrate
├── examples/scene_demo/          Scene V1/V2/V3
├── examples/type_binding_demo/   WorldSettings / QuestState
├── benchmarks/                  Timing and allocation tracking
├── tests/                       Unit and CLI integration tests
│   └── golden/                  Independent encoder and binary fixtures
├── .github/workflows/ci.yml      macOS / Ubuntu matrix
└── docs/                        Documentation, screenshots, evidence
```

`build/` holds local build and demo output and is ignored by Git. Public headers separate byte I/O, object records, bindings, scene models, and compatibility conversion; `src/` provides non-template implementations.

| Target                         | Responsibility                                      | Dependencies         |
| ------------------------------ | --------------------------------------------------- | -------------------- |
| `ChronicleRuntime`             | Binary I/O, bindings, validation, files, migrations | None                 |
| `ChronicleTools` / `chronicle` | CLI, inspection, Scene diff and JSON                | nlohmann/json 3.11.3 |
| `scene_demo`                   | Save a scene and reload in another process          | Runtime              |
| `type_binding_demo`            | Bind, save, load, and inspect external models       | Runtime / Tools      |
| `chronicle_benchmark`          | Timing and dynamic allocation metrics               | Runtime              |
| `chronicle_tests`              | Unit, compatibility, and CLI integration tests      | GoogleTest 1.15.2    |

FetchContent pins tool and test dependencies by version and SHA-256; first configuration requires network access. The runtime has no third-party runtime dependencies.

## From C++ Objects to Archives

```text
C++ model + TypeBinding<T>
          ↓
Schema + typed validation + tagged object records
          ↓
Little-endian archive → temporary file → destination file
          ↑
Bounded parsing → typed decoding → legacy conversion → current model
```

`type_binding.hpp` declares members through stable type IDs, version ranges, and `bindField(id, name, &T::member)`. The same declarations generate schemas and drive encoding, decoding, and validation. Supported values include uint64, int32, int64, finite float, UTF-8 strings, bound objects, and vectors of supported values, including nested vectors. Member ownership and unsupported types are checked at compile time.

Applications use `serializeBound`, `deserializeBound<T>`, `saveBound`, and `loadBound<T>`; `registerBinding<T>` registers dependencies. The example adds WorldSettings (ID 11) and QuestState (ID 10) without modifying core codecs.  Generic bindings read and write their declared model; Scene convenience APIs additionally select legacy DTOs and convert them to the current model.

## Current Model and Compatibility

Scene contains a name and Player. Player contains an ID, name, `Stats { health, maxHealth }`, Vec3 position, and an Item inventory. Stable type IDs are Scene=1, Player=2, Vec3=3, Item=4, and Stats=5.

| Property             | Player V1                | Player V2         | Player V3               |
| -------------------- | ------------------------ | ----------------- | ----------------------- |
| Name                 | Absent; loads as Unknown | Field ID 2        | Field ID 2              |
| Health               | Field ID 3, int32        | Field ID 3, int64 | Stats field ID 1, int64 |
| Maximum health       | Absent                   | Absent            | Stats field ID 2, int64 |
| Stats object         | Absent                   | Absent            | Player field ID 6       |
| Position / inventory | Field IDs 4 / 5          | Field IDs 4 / 5   | Field IDs 4 / 5         |

The file format and root Scene schema remain version 1; Stats schema is 1. Player field ID 3 is retired in V3. Fields carry an ID, wire type, and payload length and are written in ID order. V1/V2 golden bytes remain compatible.

Saving defaults to V3. Loading V1 widens signed health and supplies Unknown, then converts V2 to V3 with `maxHealth = max(100, health)`. Negative health is preserved without clamping. V3 requires `maxHealth > 0` and `health <= maxHealth`. Missing fields retain model defaults; invalid combinations still fail validation.

Migration supports V1→V2, V2→V3, and V1→V3. The direct route encodes and validates a V2 intermediate and installs only the final archive. Destinations must not exist; same-version migration and downgrades fail. Explicit V1/V2 saving rejects a custom maxHealth that would be lost. V1 still omits the name and requires int32 health. Both C++ and JSON use nested `player.stats`.

## Safety, Ownership, and Performance

Parsing checks boundaries, magic, versions, wire types, duplicate fields, UTF-8, and finite floats. Limits are 64 MiB per file, 1 MiB per string, 1,024 fields per object, 100,000 elements per array, and binding nesting depth 32. Unknown fields are skipped by length and discarded on re-save. Unsupported schema versions are rejected.

Borrowed ObjectView / FieldView records reduce parsing copies; loaded objects and root records own their data. Writes use bulk integer encoding and reserved capacity. ByteBuffer snapshots aliases only for self-append; file input storage moves into ByteBuffer.

Saving completes a temporary file before installing the destination. Migration failures protect the source and existing destination. There is no fsync, so power-loss durability is not guaranteed.

The benchmark defaults to 10,000 items; `--suite` covers 10 / 1,000 / 10,000 / 100,000. A fixed seed, 31 timed samples, and a separate allocation sample report medians, allocation counts, total requested bytes, and peak live requested bytes. That peak is not RSS. 

## Build and Tools

Requires CMake 3.28+, Ninja, and a C++20 compiler. Run from the project root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
./build/debug/chronicle --help
```

Configure/build presets are debug, release, relwithdebinfo, and sanitizers. Test presets are debug, release, and sanitizers.

| CLI command                              | Purpose                                                         |
| ---------------------------------------- | --------------------------------------------------------------- |
| `inspect FILE`                           | Objects, fields, schemas, sizes, and offsets                    |
| `validate FILE`                          | Parse and validate a Scene archive                              |
| `diff OLD NEW`                           | Normalized scene fields and structural changes; arrays by index |
| `to-json FILE`                           | Current-model JSON with nested Stats                            |
| `migrate SOURCE NEW [--to-version 2\|3]` | Forward migration, default V3                                   |

On 2026-10-12, all **72 CTest tests passed in each of Debug, Release, and ASan/UBSan** locally. Golden fixtures use an independent Python encoder. GitHub Actions has a configuration; no remote execution result was verified in this run.

Scope is a standalone save library and CLI. There is no GUI, Unreal plugin, automatic C++ reflection, pointer graph support, general migration graph, or automatic JSON/diff for external types. The ordinary chronicle CLI targets Scene; the external-type demo supplies its own bindings explicitly.
