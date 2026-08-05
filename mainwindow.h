#pragma once

#include "communicationstatistics.h"

#include <QByteArray>
#include <QList>
#include <QMainWindow>
#include <QVector>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QEvent;
class QFormLayout;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPlainTextEdit;
class QPushButton;
class QSerialPort;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QTimer;
class QTcpServer;
class QTcpSocket;
QT_END_NAMESPACE

class TrendChartWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void refreshPorts();
    void toggleSerialPort();
    void sendRequest();
    void sendRawFrame();
    void readSerialData();
    void readTcpData();
    void acceptTcpClient();
    void readInternalChannelData();
    void acceptInternalChannelClient();
    void processReceivedFrame();
    void handleResponseTimeout();
    void handleSerialError();
    void updateFunctionUi();
    void updateModeUi();
    void updateProtocolUi();
    void togglePolling(bool enabled);
    void refreshSlaveDataTable();
    void updateSlaveDataFromTable(int row, int column);
    void clearLog();
    void saveLog();
    void toggleTheme();
    void launchNewInstance();
    void updateResultViewMode();
    void updateSimulationUi();
    void toggleSimulation(bool enabled);
    void generateSimulationData();
    void resetSimulationPhase();
    void restoreSimulationData();
    void checkForUpdates();

private:
    void buildUi();
    void applyStyle();
    void refreshResultPanel();
    void appendTrendSample();
    void clearTrendData();
    void updateTrendSummary(const QString &message = QString());
    void clearResultPanel();
    int resultPanelColumnCount() const;
    QString formatPlcAddress(int protocolAddress) const;
    void setSlaveDataValue(int area, int address, quint16 value);
    void loadSettings();
    void saveSettings();
    void setConnectedState(bool connected);
    void setBusyState(bool busy);
    void closeInternalChannel();
    void stopSimulation(const QString &message = QString());
    bool isAddressSimulated(int area, int address) const;
    void writeFrame(const QByteArray &frame, const QString &description);
    void processWireFrame(const QByteArray &wireFrame);
    bool parseResponse(const QByteArray &frame);
    void handleSlaveRequest(const QByteArray &frame);
    void sendSlaveResponse(const QByteArray &frame, const QString &description,
                           bool successfulTransaction = true);
    void appendLog(const QString &direction, const QByteArray &data,
                   const QString &message = QString(), bool isError = false);
    void recordCommunicationResult(bool success);
    void updateCommunicationStatistics();
    void setStatus(const QString &text, bool error = false);
    bool isSlaveMode() const;
    bool isTcpMode() const;
    bool isAsciiMode() const;
    bool isInternalChannelMode() const;
    bool isTransportOpen() const;

    QByteArray buildRequest(QString *errorMessage) const;
    static quint16 modbusCrc(const QByteArray &data);
    static QByteArray appendCrc(QByteArray data);
    static bool hasValidCrc(const QByteArray &data);
    static QString toHex(const QByteArray &data);
    static QByteArray parseHex(const QString &text, bool *ok);
    static QList<quint16> parseValues(const QString &text, bool *ok);
    static QString exceptionText(quint8 code);
    static quint8 modbusLrc(const QByteArray &data);
    static QByteArray encodeAscii(const QByteArray &payload);
    static QByteArray decodeAscii(const QByteArray &frame, bool *ok);
    QByteArray encodeTcp(const QByteArray &payload, quint16 transactionId) const;

    QSerialPort *m_serial = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QTcpSocket *m_slaveTcpClient = nullptr;
    QTcpSocket *m_internalSocket = nullptr;
    QTcpServer *m_internalServer = nullptr;
    QTcpSocket *m_internalSlaveClient = nullptr;
    QTimer *m_responseTimer = nullptr;
    QTimer *m_frameGapTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_simulationTimer = nullptr;
    QNetworkAccessManager *m_updateManager = nullptr;
    QByteArray m_receiveBuffer;
    QByteArray m_tcpReceiveBuffer;
    QByteArray m_lastRequest;
    bool m_waitingForResponse = false;
    bool m_rawRequest = false;
    quint64 m_txCount = 0;
    quint64 m_rxCount = 0;
    CommunicationStatistics m_communicationStatistics;
    quint16 m_transactionId = 0;
    quint16 m_pendingTransactionId = 0;
    quint16 m_currentTransactionId = 0;
    bool m_darkTheme = false;
    bool m_resultIsBitData = false;
    int m_resultAddressArea = -1;
    int m_resultPanelColumns = 0;
    quint64 m_simulationTick = 0;
    int m_simulationArea = -1;
    int m_simulationBackupStart = 0;
    QVector<quint16> m_simulationBackup;
    QVector<quint16> m_simulationPreviousValues;
    QVector<quint8> m_coils;
    QVector<quint8> m_discreteInputs;
    QVector<quint16> m_holdingRegisters;
    QVector<quint16> m_inputRegisters;

    QComboBox *m_protocolCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_flowControlCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QPushButton *m_newWindowButton = nullptr;
    QLabel *m_connectionBadge = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_tcpPortSpin = nullptr;
    QLineEdit *m_channelNameEdit = nullptr;
    QList<QWidget *> m_serialFieldContainers;
    QList<QWidget *> m_tcpFieldContainers;
    QList<QWidget *> m_internalFieldContainers;

    QSpinBox *m_slaveSpin = nullptr;
    QSpinBox *m_slaveUnitSpin = nullptr;
    QComboBox *m_functionCombo = nullptr;
    QSpinBox *m_addressSpin = nullptr;
    QSpinBox *m_quantitySpin = nullptr;
    QLabel *m_dataLabel = nullptr;
    QLineEdit *m_writeDataEdit = nullptr;
    QPushButton *m_sendButton = nullptr;
    QCheckBox *m_pollCheck = nullptr;
    QSpinBox *m_pollIntervalSpin = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
    QStackedWidget *m_controlStack = nullptr;
    QWidget *m_masterControls = nullptr;
    QWidget *m_slaveControls = nullptr;
    QWidget *m_slavePage = nullptr;
    QLabel *m_requestTitle = nullptr;

    QComboBox *m_slaveAreaCombo = nullptr;
    QSpinBox *m_slaveViewStartSpin = nullptr;
    QSpinBox *m_slaveViewCountSpin = nullptr;
    QPushButton *m_slaveRefreshButton = nullptr;
    QGroupBox *m_simulationGroup = nullptr;
    QCheckBox *m_simulationEnabledCheck = nullptr;
    QComboBox *m_simulationModeCombo = nullptr;
    QSpinBox *m_simulationStepSpin = nullptr;
    QSpinBox *m_simulationProbabilitySpin = nullptr;
    QComboBox *m_simulationDirectionCombo = nullptr;
    QFormLayout *m_simulationParameterForm = nullptr;
    QPushButton *m_simulationResetButton = nullptr;
    QPushButton *m_simulationRestoreButton = nullptr;

    QTableWidget *m_resultTable = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_resultTitleLabel = nullptr;
    QComboBox *m_resultViewCombo = nullptr;
    QStackedWidget *m_resultViewStack = nullptr;
    QScrollArea *m_resultPanelScroll = nullptr;
    QWidget *m_resultPanelContent = nullptr;
    QGridLayout *m_resultPanelLayout = nullptr;
    TrendChartWidget *m_trendChart = nullptr;
    QLabel *m_trendSummaryLabel = nullptr;
    QCheckBox *m_trendPauseCheck = nullptr;
    QSpinBox *m_trendPointLimitSpin = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QLineEdit *m_rawEdit = nullptr;
    QCheckBox *m_autoCrcCheck = nullptr;
    QPushButton *m_rawSendButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_counterLabel = nullptr;
    QLabel *m_statisticsLabel = nullptr;
};
