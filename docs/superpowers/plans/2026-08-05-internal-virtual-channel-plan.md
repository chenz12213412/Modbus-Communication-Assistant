# 内部虚拟通道 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将虚拟串口改为仅供本机软件实例互通的内部虚拟通道，不再依赖系统 COM 端口或驱动。

**Architecture:** 使用 `QTcpServer`/`QTcpSocket` 绑定 `127.0.0.1`。通道名称经稳定哈希映射到本地端口；从站监听，主站连接。现有 Modbus RTU 帧处理、超时、解析和曲线逻辑复用 TCP/串口之外的上层流程。

**Tech Stack:** Qt Widgets、Qt Network、Qt SerialPort、C++17、CMake/qmake。

---

### Task 1: 建立内部通道状态和端口映射

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] 增加内部协议识别、通道名称控件、内部 socket/server 成员和稳定端口映射方法。
- [ ] 写一个无副作用的名称规范化与哈希映射函数，限制名称长度和字符集，端口范围使用 42000-49999。
- [ ] 增加内部通道的打开、关闭、连接成功、断开和错误处理入口。

### Task 2: 接入界面和通讯字段切换

**Files:**
- Modify: `mainwindow.cpp`
- Modify: `styles/industrial.qss`
- Modify: `styles/industrial-dark.qss`

- [ ] 协议下拉框增加“内部虚拟通道”。
- [ ] 选择内部通道时显示通道名称和“创建/加入通道”语义，隐藏无关串口字段和旧虚拟串口按钮。
- [ ] 删除 com0com 菜单、驱动说明和管理员提权提示。
- [ ] 更新亮色和暗色主题中通道输入控件的间距、可见状态和按钮样式。

### Task 3: 复用 Modbus 帧读写

**Files:**
- Modify: `mainwindow.cpp`

- [ ] 让内部 socket 的 readyRead 进入现有 RTU 接收缓冲和帧解析流程。
- [ ] 让主站发送、从站响应和原始帧发送根据当前协议选择内部 socket。
- [ ] 确保轮询、响应超时、趋势曲线、日志和连接徽标在内部通道上保持现有行为。
- [ ] 处理通道不存在、端口占用、重复创建、连接失败和对端断开。

### Task 4: 文档、构建和回归验证

**Files:**
- Modify: `README.md`
- Modify: `PROJECT_DOCUMENTATION.md`
- Modify: `CMakeLists.txt`
- Modify: `ModbusSerialAssistant.pro`

- [ ] 删除驱动依赖说明，记录内部通道使用方法和限制。
- [ ] 确认 Qt Network 已在 CMake/qmake 链接配置中声明。
- [ ] 构建 Release 版本，启动检查窗口和协议切换。
- [ ] 使用两个进程验证同名内部通道主从连接、RTU 请求响应和日志输出。
- [ ] 扫描修改文本文件的乱码、BOM 和行尾，运行 `git diff --check`。
