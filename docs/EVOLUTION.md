# Chronicle Lite：项目演进记录

## 2026-9-12 E001：性能与内存优化

### 改动与文件

| 文件                                                       | 修改内容                                                                                             |
| -------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| `include/chronicle/byte_buffer.hpp`                      | 外部数据直接追加；全量/部分自追加才创建快照；新增容量预留和接管移动向量的构造函数                                                        |
| `include/chronicle/binary_writer.hpp`、`src/binary.cpp`   | 整数在栈上编码为固定长度小端字节数组后一次写出；新增受 64 MiB 限制的 `reserve`                                                 |
| `include/chronicle/schema.hpp`、`src/schema.cpp`          | 新增借用型 `FieldView` / `ObjectView`、`decodeObjectView` 和显式拥有化 `ownObject`；对象编码预计算长度后预留容量；字段列表一次预留容量 |
| `src/schema.cpp`                                         | 解析时用最多 1,024 个 Field ID 的栈数组检查重复，取消每个字段对应的树节点分配                                                  |
| `src/serialization.cpp`                                  | 嵌套解析借用输入字节，完成解析后仅为需要返回的根记录创建拥有型 Payload；最终文件写入预留容量                                               |
| `src/file.cpp`                                           | 将读入的向量移动给 ByteBuffer，取消中间字节复制                                                                    |
| `tools/cli/output.cpp`                                   | Inspect 使用借用型嵌套解析，避免逐层复制 Payload                                                                 |
| `benchmarks/main.cpp`                                    | 默认维持 10,000 个道具，新增 `--suite` 四种规模和每种操作的分配统计；校验结果并计入结果析构                                          |
| `benchmarks/allocation_tracking.hpp/.cpp`                | 仅链接到 Benchmark 的 C++ 分配计数器，支持普通/数组/对齐/nothrow 分配；计量请求字节，不计入 Runtime 依赖                           |
| `tests/binary_tests.cpp`、`tests/serialization_tests.cpp` | 新增自追加、外部所有权、向量接管、容量/大小限制、借用与拥有型结果生命周期、最大字段数和末尾重复字段回归测试                                           |
| `CMakeLists.txt`                                         | 链接 Benchmark 计数器，并增加其普通/对齐分配及跨测量释放的自检                                                            |

#### 10,000 个道具的对比结果如下

文件大小保持为 75万 字节。

| 操作   | 旧中位数       | 新中位数       | 旧分配次数 → 新分配次数    | 旧请求字节峰值 → 新峰值           |
| ---- | ---------- | ---------- | ---------------- | ----------------------- |
| 编码   | 16.7795 ms | 2.38429 ms | 830,315 → 70,041 | 2,790,185 → 2,042,403 B |
| 解码   | 2.47417 ms | 1.06825 ms | 80,040 → 10,007  | 2,243,078 → 1,067,799 B |
| 文件保存 | 19.6737 ms | 2.88738 ms | 830,322 → 70,048 | 2,790,185 → 2,042,403 B |
| 文件加载 | 2.88421 ms | 1.15879 ms | 80,044 → 10,009  | 2,990,797 → 1,815,518 B |

该规模编码操作约为原来的 **7.04 倍速度**，编码分配次数减少 **91.56%**；解码活跃请求字节峰值减少 **52.40%**。这些是本次测量结果，不是跨机器的性能保证。

#### 不同规模的耗时

每个单元格均为“旧 → 新”，单位 ms。

| 道具数     | 编码                  | 解码                  | 文件保存                | 文件加载                |
| ------- | ------------------- | ------------------- | ------------------- | ------------------- |
| 10      | 0.027584 → 0.008167 | 0.004125 → 0.003791 | 0.302792 → 0.441542 | 0.016291 → 0.020584 |
| 1,000   | 1.81117 → 0.372667  | 0.258709 → 0.143166 | 1.92992 → 0.596875  | 0.267875 → 0.147292 |
| 10,000  | 16.7795 → 2.38429   | 2.47417 → 1.06825   | 19.6737 → 2.88738   | 2.88421 → 1.15879   |
| 100,000 | 170.325 → 23.2828   | 26.8453 → 12.8423   | 174.559 → 27.0638   | 28.1388 → 13.8396   |

10 个道具的小文件保存/加载计时本次变慢，尽管分配次数下降；该规模文件系统开销和运行噪声占比较大。本次不足以确定变慢的原因，不宣称所有输入和操作都更快。100,000 个道具编码分配次数从 8,300,098 降至 700,044；解码请求字节峰值从 22,422,932 降至 10,674,417 B。

## 2026-9-22 E002 ：显式类型绑定

### 改动与文件

| 文件                                                            | 修改内容                                                                                        |
| ------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `include/chronicle/type_binding.hpp`                          | `BoundObject` / `BoundValue` Concepts、成员字段声明、版本激活规则、基础值/对象/vector 编解码、缓存 Schema、依赖注册和绑定冲突检查 |
| `include/chronicle/scene_bindings.hpp`                        | 显式绑定 Vec3、Item、Player、Scene；Player 名字从 V2 激活，生命值在 V1 使用受范围检查的 int32 表达                      |
| `include/chronicle/archive.hpp`、`src/archive.cpp`             | 提取通用文件头与 Archive 封装，支持绑定声明指定的根 Type ID，保留原有长度/版本/Magic 检查                                   |
| `include/chronicle/bound_serialization.hpp`                   | 通用 `serializeBound` / `deserializeBound` / `saveBound` / `loadBound`，返回拥有型值与根记录             |
| `include/chronicle/serialization.hpp`、`src/serialization.cpp` | 保留 Scene 的公开 API，转为通用绑定层的薄封装；保留 `LoadedScene.playerVersion`                                 |
| `include/chronicle/schema.hpp`、`src/schema.cpp`               | 内置 Schema 从绑定生成；新增数组元素 Wire Type 元数据和元数据比较；验证数组元素类型                                         |
| `tools/cli/commands.hpp`、`tools/cli/output.cpp`               | 新增接受调用者 SchemaRegistry 的 `inspectRecord`；展开对象数组，显示基础值/嵌套数组的元素数                              |
| `examples/type_binding_demo/model.hpp`、`main.cpp`             | 独立业务类型、绑定声明和保存/加载/检查示例，无专用序列化算法                                                             |
| `tests/binding_tests.cpp`、`CMakeLists.txt`                    | 新增 13 项绑定测试及 3 项独立进程示例测试，加入新 Runtime 源文件与示例构建目标                                             |
| `docs/TYPE_BINDINGS.md` 与中英文项目/演示说明                           | 说明绑定声明、支持类型、公共接口、版本规则、所有权和扩展示例；保留历史 E001 与原始截图                                              |

### 性能检查

| 操作   | E001 中位数   | E002 中位数   | 分配次数 E001 → E002 | 活跃请求字节峰值 E001 → E002    |
| ---- | ---------- | ---------- | ---------------- | ----------------------- |
| 编码   | 2.50483 ms | 2.26571 ms | 70,041 → 60,035  | 2,042,403 → 2,042,363 B |
| 解码   | 1.09258 ms | 1.06667 ms | 10,007 → 10,007  | 1,067,799 → 1,067,799 B |
| 文件保存 | 2.86646 ms | 2.77829 ms | 70,048 → 60,042  | 2,042,403 → 2,042,363 B |
| 文件加载 | 1.17283 ms | 1.21721 ms | 10,009 → 10,009  | 1,815,518 → 1,815,518 B |

文件仍为 747,719 字节。字段列表一次预留容量，避免旧 initializer-list 的部分拥有型字段复制，编码分配次数下降；泛型数组的逐项 Payload 临时缓冲使编码累计请求字节从 7,622,643 增至 8,022,289 B

## 2026-10-7 E003：V3 结构迁移

| 来源     | 兼容加载到当前模型   | 迁移到 V2 | 迁移到 V3         |
| ------ | ----------- | ------ | -------------- |
| V1     | 经过 V2、V3 转换 | 支持     | 支持，经验证的 V2 中间档 |
| V2     | 经过 V3 转换    | 拒绝同版本  | 支持             |
| V3     | 直接读取        | 拒绝降级   | 拒绝同版本          |
| 不支持的版本 | 拒绝          | 拒绝     | 拒绝             |

### 修改的文件和行为

| 文件                                                            | 改动                                                         |
| ------------------------------------------------------------- | ---------------------------------------------------------- |
| `include/chronicle/scene.hpp`、`schema.hpp`                    | 新增 Stats，当前 Player 改用 stats，新增稳定 Stats Type ID 5           |
| `include/chronicle/migration.hpp`、`src/migration.cpp`         | 旧版 DTO、两个纯转换边、Scene 转换及受检查的旧格式投影                           |
| `include/chronicle/scene_bindings.hpp`、`src/schema.cpp`       | 各代使用独立绑定，内置 Registry 注册三代 Player 与 Stats；不复用退役 health ID   |
| `include/chronicle/type_binding.hpp`                          | Player 写入选项默认 V3；通用字段编码规则未改变                               |
| `include/chronicle/serialization.hpp`、`src/serialization.cpp` | 默认保存 V3；按来源选择旧 DTO 并返回当前模型；新增 `migrateArchive` 和中间/最终档往返验证 |
| `src/file.cpp`                                                | 将已验证迁移结果安装到不存在的新目标，保持源文件、现有目标与临时文件失败保护                     |
| `tools/cli/commands.cpp`                                      | migrate 默认目标 V3，可显式 `--to-version 2` 或 `3`                 |
| `tools/cli/output.cpp`                                        | JSON 导出当前嵌套 Stats；Diff 报告字段搬迁和 maxHealth 新增/移除             |
| `examples/scene_demo/main.cpp`                                | 可保存 V1/V2/V3，默认 V3；加载显示 health 与 maxHealth                 |
| `tests/golden/generate.py`、`scene_v3.bin`                     | 独立编码器生成 V3 样本，四个历史二进制样本不变                                  |
| `tests/migration_v3_tests.cpp`、既有消费者测试                        | 新增结构、转换、边界和失败测试；现有测试改用当前 stats 成员和显式旧版本目标                  |
| `tests/cli_workflow.cmake`、`CMakeLists.txt`                   | 独立进程验证直达/分步一致和 Stats Inspect，增加 V3 Golden CLI 验证           |


