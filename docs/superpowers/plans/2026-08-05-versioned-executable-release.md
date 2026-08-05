# 带版本号 EXE 发布实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 生成并保留 `v1.0.5`、`v1.0.6`、`v1.0.7` 普通版与独立版 EXE，在文件名和 Windows 文件属性中写入版本，并仅发布 `v1.0.7`。

**Architecture:** CMake 根据项目版本配置 Windows `VERSIONINFO` 资源；版本化打包脚本校验已构建 EXE 的版本属性，再生成不可覆盖的普通版和独立版归档，同时刷新无版本号最新版副本。独立版 SFX 复用普通版的图标和版本资源。

**Tech Stack:** Qt 6、CMake、MinGW、Windows RC、PowerShell、7-Zip SFX、Resource Hacker、GitHub CLI。

---

### Task 1: Windows 版本属性回归测试

**Files:**
- Create: `tests/versioned_executable_test.ps1`

- [ ] **Step 1: 编写失败测试**

脚本接收 `-Executable` 和 `-ExpectedVersion`，读取 `(Get-Item $Executable).VersionInfo`，要求 `FileVersion` 与 `ProductVersion` 均以 `$ExpectedVersion` 开头，并在不一致时退出 1。

- [ ] **Step 2: 确认当前包测试失败**

运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\versioned_executable_test.ps1 -Executable dist\ModbusSerialAssistant.exe -ExpectedVersion 1.0.5
```

预期：FAIL，当前 `app_icon.rc` 只有图标资源，没有版本资源。

### Task 2: CMake 生成 VERSIONINFO

**Files:**
- Create: `app_version.rc.in`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建版本资源模板**

模板使用 `@PROJECT_VERSION_MAJOR@`、`@PROJECT_VERSION_MINOR@`、`@PROJECT_VERSION_PATCH@` 生成数值版本 `x,y,z,0`，并写入中文产品名、文件说明、原始文件名和字符串版本 `x.y.z.0`。

- [ ] **Step 2: 接入 CMake**

在 `add_executable` 前执行：

```cmake
configure_file(app_version.rc.in app_version.rc @ONLY)
```

Windows 构建源改为 `${CMAKE_CURRENT_BINARY_DIR}/app_version.rc`，模板同时包含应用图标资源。

- [ ] **Step 3: 构建并确认测试通过**

重新配置并构建 `ModbusSerialAssistant`，对生成的普通版执行 Task 1 测试，预期 PASS 且版本为 `1.0.5.0`。

- [ ] **Step 4: 提交版本资源改动**

```powershell
git add CMakeLists.txt app_version.rc.in tests\versioned_executable_test.ps1
git commit -m "build: embed Windows executable version"
```

### Task 3: 可复用版本化打包

**Files:**
- Create: `packaging/build-versioned-release.ps1`
- Modify: `packaging/onefile/build-onefile.ps1`
- Modify: `tests/versioned_executable_test.ps1`

- [ ] **Step 1: 扩展测试覆盖文件命名和独立版属性**

测试要求普通版路径为 `ModbusSerialAssistant_v<version>.exe`，独立版路径为 `ModbusSerialAssistant_v<version>_OneFile.exe`，并验证两者 `FileVersion` 和 `ProductVersion`。

- [ ] **Step 2: 确认独立版版本属性测试失败**

对当前独立包执行测试，预期 FAIL，因为 SFX 外壳尚未复制 `VERSIONINFO`。

- [ ] **Step 3: 为 SFX 写入版本资源**

`build-onefile.ps1` 在写入图标后，再用 Resource Hacker 从已构建普通版复制 `VERSIONINFO,1,` 到 SFX 模块，然后组合独立包。

- [ ] **Step 4: 新增版本化发布脚本**

`build-versioned-release.ps1 -Version x.y.z` 完成：校验构建 EXE 属性、拒绝覆盖已有版本归档、生成两个带版本号文件、验证两者属性、最后刷新无版本号快捷副本。

- [ ] **Step 5: 运行脚本测试并提交**

预期普通版和独立版属性测试均 PASS。

```powershell
git add packaging\build-versioned-release.ps1 packaging\onefile\build-onefile.ps1 tests\versioned_executable_test.ps1
git commit -m "build: preserve versioned release executables"
```

### Task 4: 归档 v1.0.5 并生成 v1.0.6

**Files:**
- Modify: `main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `ModbusSerialAssistant.pro`
- Output: `dist/ModbusSerialAssistant_v1.0.5.exe`
- Output: `dist/ModbusSerialAssistant_v1.0.5_OneFile.exe`
- Output: `dist/ModbusSerialAssistant_v1.0.6.exe`
- Output: `dist/ModbusSerialAssistant_v1.0.6_OneFile.exe`

- [ ] **Step 1: 重新构建并归档 v1.0.5**

使用新增版本资源重新构建 `1.0.5`，运行版本化打包脚本生成两个归档文件。

- [ ] **Step 2: 将三个源码版本号更新为 1.0.6**

更新 `QApplication::setApplicationVersion`、CMake `project(VERSION)` 和 qmake `VERSION`。

- [ ] **Step 3: 完整构建、测试和打包**

运行 CMake 构建、全部 CTest 和版本化打包脚本，预期 4/4 测试通过且两个 `v1.0.6` 文件属性为 `1.0.6.0`。

- [ ] **Step 4: 提交 v1.0.6**

```powershell
git add main.cpp CMakeLists.txt ModbusSerialAssistant.pro
git commit -m "release: prepare version 1.0.6"
```

### Task 5: 生成并发布 v1.0.7

**Files:**
- Modify: `main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `ModbusSerialAssistant.pro`
- Output: `dist/ModbusSerialAssistant_v1.0.7.exe`
- Output: `dist/ModbusSerialAssistant_v1.0.7_OneFile.exe`

- [ ] **Step 1: 将三个源码版本号更新为 1.0.7**

- [ ] **Step 2: 完整构建、测试和打包**

预期 4/4 测试通过，两个 `v1.0.7` 文件属性为 `1.0.7.0`，六个版本归档文件同时存在。

- [ ] **Step 3: 提交并同步 GitHub main**

```powershell
git add main.cpp CMakeLists.txt ModbusSerialAssistant.pro
git commit -m "release: prepare version 1.0.7"
```

- [ ] **Step 4: 创建 GitHub v1.0.7 Release**

上传：

```text
dist/ModbusSerialAssistant_v1.0.7.exe
dist/ModbusSerialAssistant_v1.0.7_OneFile.exe
```

- [ ] **Step 5: 端到端验证自动更新**

启动 `ModbusSerialAssistant_v1.0.6_OneFile.exe`，枚举其顶层窗口；预期出现标题“发现新版本”，正文包含 `1.0.7`。关闭测试进程，不执行实际覆盖。

- [ ] **Step 6: 最终质量检查**

扫描本次文本文件乱码特征和裸 LF，执行 `git diff --check`，核对 GitHub 最新 Release、资源名、六个本地归档及无版本号 `v1.0.7` 快捷副本。
