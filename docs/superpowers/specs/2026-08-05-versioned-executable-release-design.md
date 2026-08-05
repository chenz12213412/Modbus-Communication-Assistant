# 带版本号 EXE 发布设计

## 目标

生成并保留 Modbus 通讯助手 `v1.0.5`、`v1.0.6` 和 `v1.0.7` 的普通版与独立版 EXE。文件名和 Windows 文件属性均包含准确版本号，仅将 `v1.0.7` 发布到 GitHub，并使用本地 `v1.0.6` 验证自动更新。

## 文件命名

每个版本生成两个不可覆盖的归档文件：

- `ModbusSerialAssistant_v1.0.6.exe`
- `ModbusSerialAssistant_v1.0.6_OneFile.exe`

`v1.0.5` 和 `v1.0.7` 使用相同命名规则。`dist` 中继续保留 `ModbusSerialAssistant.exe` 与 `ModbusSerialAssistant_OneFile.exe`，作为当前最新版快捷副本。

GitHub `v1.0.7` Release 上传两个带版本号的文件。独立版文件名仍以 `_OneFile.exe` 结尾，兼容现有自动更新资源选择逻辑。

## Windows 文件属性

CMake 构建时生成 Windows `VERSIONINFO` 资源，并写入：

- 文件版本和产品版本：`1.0.6.0` 或 `1.0.7.0`
- 产品名称：`Modbus 通讯助手`
- 文件说明：`Modbus 通讯助手`
- 原始文件名：`ModbusSerialAssistant.exe`

应用界面、CMake 项目、qmake 项目和 Windows 文件属性使用同一个发布版本，构建后通过 PowerShell 读取 EXE 的 `VersionInfo` 验证一致性。

## 构建与保留流程

1. 将当前 `v1.0.5` 普通版和独立版复制为带版本号的归档文件。
2. 更新源码版本为 `1.0.6`，构建、测试并生成两个 `v1.0.6` 归档文件。
3. 更新源码版本为 `1.0.7`，重新构建、测试并生成两个 `v1.0.7` 归档文件。
4. 不删除或覆盖已归档的旧版本文件。
5. 无版本号快捷副本最终指向 `v1.0.7`。

## GitHub 与自动更新

仅创建 GitHub `v1.0.7` Release，并上传两个 `v1.0.7` 文件。启动本地 `v1.0.6` 独立版，等待其读取 GitHub 最新 Release；成功标准是出现“发现新版本”窗口并显示 `1.0.7`。

## 验证

- 完整 CMake 构建成功，现有测试全部通过。
- 六个版本化 EXE 文件均存在且旧版本未被覆盖。
- `v1.0.6` 和 `v1.0.7` 的普通版与独立版文件名包含版本号。
- 普通版内部文件属性分别显示 `1.0.6.0` 和 `1.0.7.0`。
- 独立版启动后的应用界面分别显示 `v1.0.6` 和 `v1.0.7`。
- GitHub 最新 Release 为 `v1.0.7`，资源文件名带版本号。
- `v1.0.6` 能检测到 `v1.0.7` 更新。
