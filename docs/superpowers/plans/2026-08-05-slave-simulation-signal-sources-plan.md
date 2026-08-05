# 从站仿真范围简化与信号源扩展 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让从站自动仿真复用显示范围，固定 500 ms 更新和 10 s 波形周期，并增加四种寄存器信号源。

**Architecture:** `SlaveDataSimulator` 保持为纯计算组件，新增信号源枚举和显式状态输入；`MainWindow` 使用从站显示起始地址/数量构造范围，持有随机游走状态并管理固定定时器。删除旧仿真范围和周期控件及其配置读写，保留快照恢复和编辑锁定。

**Tech Stack:** Qt Widgets、Qt Core、C++17、CMake、qmake。

---

### Task 1: 扩展生成器测试与接口

**Files:**
- Modify: `slavedatasimulator.h`
- Modify: `slavedatasimulator.cpp`
- Modify: `tests/slavedatasimulator_test.cpp`

- [x] **Step 1: 写新增信号源失败测试**

为 `SlaveRegisterMode` 增加热噪声、随机游走、脉冲和阻尼正弦的期望行为测试；测试范围边界，并让随机游走接收上一状态。

- [x] **Step 2: 运行测试确认失败**

运行 `cmake --build X:\build-simulation --config Release --target slavedatasimulator_test`，应因枚举或生成接口尚不存在而失败。

- [x] **Step 3: 实现最小生成器改动**

增加模式枚举和固定周期常量；生成器接口增加随机游走上一状态与下一状态输出所需的显式参数，不使用全局持久状态。所有输出使用现有范围裁剪逻辑。

- [x] **Step 4: 运行仿真测试**

运行 `X:\build-simulation\slavedatasimulator_test.exe`，确认新增模式和原有模式全部通过。

### Task 2: 简化从站自动数据界面

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `styles/industrial.qss`
- Modify: `styles/industrial-dark.qss`

- [x] **Step 1: 删除重复控件成员和表单行**

移除更新周期、仿真起始地址、仿真数量和波形周期的控件及连接；保留模式、上下限、步长、概率和方向控件。

- [x] **Step 2: 固定仿真配置**

从站显示范围变化时同步停止仿真；生成配置的起始地址和数量读取 `m_slaveViewStartSpin`、`m_slaveViewCountSpin`，定时器固定使用 500 ms，波形步数固定为 20。

- [x] **Step 3: 增加新模式文案和参数可见性**

在寄存器模式下加入四种新模式。新模式只显示必要参数：热噪声显示上下限，随机游走显示上下限和步长，脉冲与阻尼正弦显示上下限；不显示被删除的周期设置。

- [x] **Step 4: 保存状态兼容处理**

停止读取并停止写入旧版 `simulation/interval`、`simulation/start`、`simulation/count`、`simulation/period` 配置；仍保存模式、上下限、步长、概率和方向。启动时仿真始终关闭。

### Task 3: 接入状态、快照和编辑锁定

**Files:**
- Modify: `mainwindow.cpp`

- [x] **Step 1: 绑定显示范围快照**

启用时按当前数据区、显示起始地址和显示数量保存快照；恢复时按同一范围写回。

- [x] **Step 2: 接入随机游走状态**

启用或重置模式时以范围中点初始化每个寄存器状态；每次定时更新后保存下一状态，模式切换或显示范围变化时清空并重置。

- [x] **Step 3: 验证编辑锁定边界**

仿真范围内表格、面板指示灯和寄存器编辑器不可编辑，范围外保持可编辑；停止后重新刷新控件状态。

### Task 4: 文档、构建和发布

**Files:**
- Modify: `README.md`
- Modify: `PROJECT_DOCUMENTATION.md`
- Modify: `docs/superpowers/plans/2026-08-05-slave-data-simulation-plan.md`

- [x] **Step 1: 更新使用说明**

说明仿真使用显示范围、固定周期和新增信号源，删除旧版重复参数描述。

- [x] **Step 2: 完成构建和测试**

运行 CMake Release 构建、`ctest --test-dir X:\build-simulation -C Release --output-on-failure`，要求所有测试通过。

- [x] **Step 3: 生成发布文件**

更新 `dist\ModbusSerialAssistant.exe` 和 `dist\ModbusSerialAssistant_OneFile.exe`，确认目录中不产生历史版本副本。

- [x] **Step 4: 执行最终检查**

检查普通版启动、单文件 7-Zip 归档完整性、`git diff --check`、UTF-8 无 BOM、CRLF 和乱码特征。
