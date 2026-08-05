# Communication Result Statistics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add bottom-right success, failure, and success-rate statistics for master and slave Modbus transactions.

**Architecture:** A small `CommunicationStatistics` value object owns counters and display formatting. `MainWindow` owns one instance, updates a dedicated status-bar label, and records exactly one terminal result per communication transaction at the existing master parsing, timeout, send, and slave response boundaries.

**Tech Stack:** C++17, Qt 6 Core/Widgets/Network/SerialPort, CMake, Qt Test.

---

### Task 1: Add the statistics value object

**Files:**
- Create: `communicationstatistics.h`
- Create: `communicationstatistics.cpp`
- Create: `tests/communicationstatistics_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `ModbusSerialAssistant.pro`

- [ ] **Step 1: Write the failing unit test**

Create tests for the initial text, success/failure increments, percentage rounding, and reset:

```cpp
CommunicationStatistics stats;
QCOMPARE(stats.displayText(), QStringLiteral("成功 0  ·  失败 0  ·  成功率 --"));
stats.recordSuccess();
stats.recordFailure();
stats.recordSuccess();
QCOMPARE(stats.displayText(), QStringLiteral("成功 2  ·  失败 1  ·  成功率 67%"));
stats.reset();
QCOMPARE(stats.successCount(), quint64(0));
QCOMPARE(stats.failureCount(), quint64(0));
```

- [ ] **Step 2: Register and run the test to verify it fails**

Run:

```powershell
cmake -S X:\ -B X:\build-final-check2 -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH='D:\Qt\6.10.2\mingw_64'
cmake --build X:\build-final-check2 --target communicationstatistics_test
```

Expected: compilation fails because `CommunicationStatistics` is not defined.

- [ ] **Step 3: Implement the value object**

Expose this focused API:

```cpp
class CommunicationStatistics {
public:
    void recordSuccess();
    void recordFailure();
    void reset();
    quint64 successCount() const;
    quint64 failureCount() const;
    QString displayText() const;
private:
    quint64 m_successCount = 0;
    quint64 m_failureCount = 0;
};
```

Calculate the integer percentage with nearest-integer rounding:

```cpp
const quint64 total = m_successCount + m_failureCount;
const quint64 percentage = (m_successCount * 100 + total / 2) / total;
```

- [ ] **Step 4: Run the focused test**

Run:

```powershell
cmake --build X:\build-final-check2 --target communicationstatistics_test
ctest --test-dir X:\build-final-check2 -R communicationstatistics_test --output-on-failure
```

Expected: `communicationstatistics_test` passes.

### Task 2: Add the bottom-right statistics label

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] **Step 1: Add state and update helpers**

Include `communicationstatistics.h`, add `CommunicationStatistics m_communicationStatistics`, `QLabel *m_statisticsLabel`, and these helpers:

```cpp
void recordCommunicationResult(bool success);
void updateCommunicationStatistics();
```

- [ ] **Step 2: Add the label to the bottom bar**

Create `m_statisticsLabel` after `m_counterLabel`, set object name `statisticsText`, initialize it from `displayText()`, and add it after the TX/RX label so it is the rightmost status item.

- [ ] **Step 3: Implement updates and reset behavior**

Implement:

```cpp
void MainWindow::recordCommunicationResult(bool success)
{
    success ? m_communicationStatistics.recordSuccess()
            : m_communicationStatistics.recordFailure();
    updateCommunicationStatistics();
}

void MainWindow::updateCommunicationStatistics()
{
    m_statisticsLabel->setText(m_communicationStatistics.displayText());
}
```

In `clearLog()`, call `m_communicationStatistics.reset()` and `updateCommunicationStatistics()` after resetting TX/RX.

### Task 3: Wire master transaction outcomes

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] **Step 1: Make response parsing return a result**

Change `parseResponse(const QByteArray &frame)` from `void` to `bool`. Return `false` for address, exception, function, length, and echo mismatches; return `true` after each valid read or write response is rendered.

- [ ] **Step 2: Record valid master responses**

In `processWireFrame()`, after protocol and CRC validation:

```cpp
if (m_rawRequest) {
    setStatus(QStringLiteral("收到有效响应，共 %1 字节").arg(frame.size()));
    recordCommunicationResult(true);
    return;
}
recordCommunicationResult(parseResponse(frame));
```

- [ ] **Step 3: Record master failures once**

For MBAP, transaction ID, LRC, short frame, and CRC errors, stop the response timer and clear `m_waitingForResponse` before recording failure when in master mode. Record failure in `writeFrame()` when the transport write fails and in `handleResponseTimeout()` when a pending request expires.

Use a guard based on `m_waitingForResponse` for master receive failures so a malformed unsolicited frame does not create a transaction result and the timeout path cannot count the same request again.

### Task 4: Wire slave transaction outcomes

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] **Step 1: Carry the intended result through slave responses**

Change the helper signature to:

```cpp
void sendSlaveResponse(const QByteArray &frame, const QString &description,
                       bool successfulTransaction = true);
```

Record failure if the transport write fails; otherwise record `successfulTransaction`. Pass `false` for Modbus exception responses and `true` for normal responses.

- [ ] **Step 2: Count invalid slave requests and broadcasts**

Record failure for malformed or checksum-invalid requests before returning. Count valid broadcast writes as success at the point where data is applied because no response is sent. Ignored requests addressed to another station and ignored broadcast reads do not affect statistics.

- [ ] **Step 3: Prevent double counting**

Ensure exception responses call only `sendSlaveResponse(..., false)` and do not record a separate failure beforehand. Ensure normal responses and valid broadcasts each have exactly one result-recording call.

### Task 5: Verify, package, and inspect

**Files:**
- Modify generated outputs under `build/release` and `dist`

- [ ] **Step 1: Run the complete build and test suite**

Run:

```powershell
cmake --build X:\build-final-check2 --config Release --clean-first
ctest --test-dir X:\build-final-check2 -C Release --output-on-failure
```

Expected: all tests pass, including `communicationstatistics_test`.

- [ ] **Step 2: Run the application smoke test**

Start `X:\build-final-check2\ModbusSerialAssistant.exe`, verify the process is responsive, and confirm the bottom-right text starts at `成功 0 · 失败 0 · 成功率 --`.

- [ ] **Step 3: Rebuild deliverables**

Copy the Release executable to `dist\ModbusSerialAssistant.exe`, run `windeployqt`, and rebuild `dist\ModbusSerialAssistant_OneFile.exe` with `packaging\onefile\build-onefile.ps1`.

- [ ] **Step 4: Run repository hygiene checks**

Verify all modified text files are UTF-8 without BOM, use CRLF line endings, contain no mojibake patterns, and pass `git diff --check`.
