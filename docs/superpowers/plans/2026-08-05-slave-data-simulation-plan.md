# 从站数据仿真 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为从站当前数据区增加可配置、可恢复的自动位状态和寄存器波形生成。

**Architecture:** 新建不依赖界面的 `SlaveDataSimulator` 负责参数校验和单次生成；`MainWindow` 使用独立 `QTimer` 将生成值写入现有四个数据向量。界面根据当前数据区切换位模式或寄存器模式参数，并在主从模式切换时动态更新“数据解析/数据仿真”标题。

**Tech Stack:** Qt Widgets、Qt Core、C++17、CMake/qmake。

---

### Task 1: 仿真引擎测试与实现

**Files:**
- Create: `slavedatasimulator.h`
- Create: `slavedatasimulator.cpp`
- Create: `tests/slavedatasimulator_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `ModbusSerialAssistant.pro`

- [x] 写失败测试，覆盖范围校验、随机值边界、流水灯、闪烁、交替翻转、正弦、方波、三角波、计数和锯齿波。
- [x] 运行测试，确认因 `SlaveDataSimulator` 尚不存在而失败。
- [x] 实现纯计算引擎，使所有模式测试通过。
- [x] 运行全部测试，确认内部通道测试仍通过。

### Task 2: 从站仿真控制界面

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `styles/industrial.qss`
- Modify: `styles/industrial-dark.qss`

- [x] 增加独立仿真定时器和自动数据分组控件。
- [x] 位数据区显示随机、流水灯、闪烁、交替、全开、全关；寄存器区显示随机、正弦、方波、三角波、计数、锯齿波。
- [x] 根据模式显示更新周期、最小/最大值、波形周期、步长、随机概率和流水方向。
- [x] 主站显示“数据解析”，从站显示“数据仿真”。

### Task 3: 数据写入、恢复与冲突处理

**Files:**
- Modify: `mainwindow.cpp`

- [x] 启用时保存仿真范围快照并按 tick 写入当前数据区。
- [x] 停止时保留最后值，恢复命令写回启用前快照。
- [x] 仿真范围内禁用表格、指示灯和寄存器编辑；范围外保持可编辑。
- [x] 切换数据区、范围、模式或主从状态时停止或重置相位，避免跨配置污染。
- [x] 保存仿真参数，但应用启动后保持未启用。

### Task 4: 文档与发布验证

**Files:**
- Modify: `README.md`
- Modify: `项目文档.md`

- [x] 补充模式、参数和手动/通讯写入规则。
- [x] 构建 Release 并运行全部测试。
- [x] 启动普通版，检查从站控件布局和主从标题切换。
- [x] 生成普通版与单文件版 EXE。
- [x] 检查 UTF-8 无 BOM、CRLF、乱码和 `git diff --check`。
