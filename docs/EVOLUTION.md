# Chronicle Lite: Project Evolution Log

## 2026-9-12 E001: Performance and Memory Optimization

### Changes and Files

| File | Changes |
| --- | --- |
| `include/chronicle/byte_buffer.hpp` | Append external data directly; create a snapshot only for full or partial self-append; add capacity reservation and a constructor that takes ownership of a moved vector |
| `include/chronicle/binary_writer.hpp`, `src/binary.cpp` | Encode integers into fixed-size little-endian byte arrays on the stack and write them in one operation; add `reserve` with a 64 MiB limit |
| `include/chronicle/schema.hpp`, `src/schema.cpp` | Add borrowed `FieldView` / `ObjectView`, `decodeObjectView`, and explicit ownership conversion through `ownObject`; precompute encoded object sizes before reserving capacity; reserve field-list capacity once |
| `src/schema.cpp` | Check duplicate fields during parsing with a stack array of up to 1,024 field IDs, eliminating per-field tree-node allocations |
| `src/serialization.cpp` | Borrow input bytes during nested parsing; after parsing, create owned payloads only for root records that must be returned; reserve capacity for the final archive write |
| `src/file.cpp` | Move the input vector into ByteBuffer, eliminating an intermediate byte copy |
| `tools/cli/output.cpp` | Use borrowed nested parsing for Inspect, avoiding payload copies at each level |
| `benchmarks/main.cpp` | Keep the default of 10,000 items; add four data sizes through `--suite` and allocation metrics for each operation; validate results and include result destruction in measurements |
| `benchmarks/allocation_tracking.hpp/.cpp` | Add a C++ allocation counter linked only to the benchmark, supporting regular, array, aligned, and nothrow allocations; measure requested bytes without adding a runtime dependency |
| `tests/binary_tests.cpp`, `tests/serialization_tests.cpp` | Add regression tests for self-append, external ownership, vector adoption, capacity/size limits, borrowed and owned result lifetimes, maximum field counts, and duplicate fields at the end |
| `CMakeLists.txt` | Link the benchmark counter and add self-checks for regular/aligned allocations and deallocation across measurement boundaries |

#### Comparison with 10,000 Items

The file size remains approximately 750,000 bytes.

| Operation | Previous median | New median | Previous allocations → New allocations | Previous peak requested bytes → New peak |
| --- | --- | --- | --- | --- |
| Encode | 16.7795 ms | 2.38429 ms | 830,315 → 70,041 | 2,790,185 → 2,042,403 B |
| Decode | 2.47417 ms | 1.06825 ms | 80,040 → 10,007 | 2,243,078 → 1,067,799 B |
| File save | 19.6737 ms | 2.88738 ms | 830,322 → 70,048 | 2,790,185 → 2,042,403 B |
| File load | 2.88421 ms | 1.15879 ms | 80,044 → 10,009 | 2,990,797 → 1,815,518 B |

At this size, encoding is approximately **7.04 times as fast**, with **91.56% fewer encoding allocations**. Peak live requested bytes during decoding decrease by **52.40%**. These are results from this measurement, not performance guarantees across machines.

#### Timing Across Data Sizes

Each cell shows “previous → new” in milliseconds.

| Items | Encode | Decode | File save | File load |
| --- | --- | --- | --- | --- |
| 10 | 0.027584 → 0.008167 | 0.004125 → 0.003791 | 0.302792 → 0.441542 | 0.016291 → 0.020584 |
| 1,000 | 1.81117 → 0.372667 | 0.258709 → 0.143166 | 1.92992 → 0.596875 | 0.267875 → 0.147292 |
| 10,000 | 16.7795 → 2.38429 | 2.47417 → 1.06825 | 19.6737 → 2.88738 | 2.88421 → 1.15879 |
| 100,000 | 170.325 → 23.2828 | 26.8453 → 12.8423 | 174.559 → 27.0638 | 28.1388 → 13.8396 |

Saving and loading the small 10-item file took longer in this run despite fewer allocations; filesystem overhead and measurement noise account for a larger proportion at this size. This run is insufficient to determine the cause of the slowdown, so no claim is made that every input and operation is faster. With 100,000 items, encoding allocations fall from 8,300,098 to 700,044, and peak requested bytes during decoding fall from 22,422,932 to 10,674,417 B.

## 2026-9-22 E002: Explicit Type Bindings

### Changes and Files

| File | Changes |
| --- | --- |
| `include/chronicle/type_binding.hpp` | Add `BoundObject` / `BoundValue` concepts, member field declarations, version activation rules, primitive/object/vector codecs, cached schemas, dependency registration, and binding conflict checks |
| `include/chronicle/scene_bindings.hpp` | Explicitly bind Vec3, Item, Player, and Scene; activate the Player name from V2 and represent V1 health as a range-checked int32 |
| `include/chronicle/archive.hpp`, `src/archive.cpp` | Extract a generic file header and archive wrapper; support root type IDs specified by bindings while retaining length, version, and magic checks |
| `include/chronicle/bound_serialization.hpp` | Add generic `serializeBound` / `deserializeBound` / `saveBound` / `loadBound`, returning owned values and root records |
| `include/chronicle/serialization.hpp`, `src/serialization.cpp` | Retain the public Scene API as a thin wrapper around generic bindings; retain `LoadedScene.playerVersion` |
| `include/chronicle/schema.hpp`, `src/schema.cpp` | Generate built-in schemas from bindings; add array element wire-type metadata and metadata comparison; validate array element types |
| `tools/cli/commands.hpp`, `tools/cli/output.cpp` | Add `inspectRecord` accepting a caller-supplied SchemaRegistry; expand object arrays and show element counts for primitive and nested arrays |
| `examples/type_binding_demo/model.hpp`, `main.cpp` | Add independent application types, bindings, and save/load/inspect examples without dedicated serialization algorithms |
| `tests/binding_tests.cpp`, `CMakeLists.txt` | Add 13 binding tests and 3 separate-process example tests; register the new runtime source and example target |
| `docs/TYPE_BINDINGS.md` and Chinese/English project and demo documentation | Describe bindings, supported types, public interfaces, version rules, ownership, and the extension example; preserve historical E001 records and original screenshots |

### Performance Check

| Operation | E001 median | E002 median | Allocations E001 → E002 | Peak live requested bytes E001 → E002 |
| --- | --- | --- | --- | --- |
| Encode | 2.50483 ms | 2.26571 ms | 70,041 → 60,035 | 2,042,403 → 2,042,363 B |
| Decode | 1.09258 ms | 1.06667 ms | 10,007 → 10,007 | 1,067,799 → 1,067,799 B |
| File save | 2.86646 ms | 2.77829 ms | 70,048 → 60,042 | 2,042,403 → 2,042,363 B |
| File load | 1.17283 ms | 1.21721 ms | 10,009 → 10,009 | 1,815,518 → 1,815,518 B |

The file remains 747,719 bytes. Reserving field-list capacity once avoids some owned-field copies from the previous initializer-list implementation, reducing encoding allocations. Per-element temporary payload buffers in generic arrays increase total requested bytes during encoding from 7,622,643 to 8,022,289 B.

## 2026-10-7 E003: V3 Structural Migration

| Source | Compatibility loading into the current model | Migrate to V2 | Migrate to V3 |
| --- | --- | --- | --- |
| V1 | Convert through V2 and V3 | Supported | Supported through a validated V2 intermediate archive |
| V2 | Convert through V3 | Same-version migration rejected | Supported |
| V3 | Read directly | Downgrade rejected | Same-version migration rejected |
| Unsupported version | Rejected | Rejected | Rejected |

### Changed Files and Behavior

| File | Changes |
| --- | --- |
| `include/chronicle/scene.hpp`, `schema.hpp` | Add Stats, change the current Player to use stats, and add stable Stats type ID 5 |
| `include/chronicle/migration.hpp`, `src/migration.cpp` | Add legacy DTOs, two pure conversion edges, Scene conversion, and checked projection into legacy formats |
| `include/chronicle/scene_bindings.hpp`, `src/schema.cpp` | Use separate bindings for each generation; register all three Player versions and Stats in the built-in registry; do not reuse the retired health ID |
| `include/chronicle/type_binding.hpp` | Default Player write options to V3; leave generic field encoding rules unchanged |
| `include/chronicle/serialization.hpp`, `src/serialization.cpp` | Save V3 by default; select legacy DTOs based on the source and return the current model; add `migrateArchive` and round-trip validation of intermediate/final archives |
| `src/file.cpp` | Install validated migration results into a new, nonexistent destination; retain failure protection for source files, existing destinations, and temporary files |
| `tools/cli/commands.cpp` | Default migrate to V3, with explicit `--to-version 2` or `3` |
| `tools/cli/output.cpp` | Export current nested Stats in JSON; report field relocation and maxHealth addition/removal in Diff |
| `examples/scene_demo/main.cpp` | Save V1/V2/V3, defaulting to V3; display health and maxHealth when loading |
| `tests/golden/generate.py`, `scene_v3.bin` | Generate the V3 fixture with the independent encoder; keep the four historical binary fixtures unchanged |
| `tests/migration_v3_tests.cpp` and existing consumer tests | Add structure, conversion, boundary, and failure tests; update existing tests to use current stats members and explicit legacy targets |
| `tests/cli_workflow.cmake`, `CMakeLists.txt` | Verify direct/staged equivalence and Stats Inspect in separate processes; add CLI validation of the V3 golden fixture |
