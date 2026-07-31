#include "mainwindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kDefaultTimeoutMs = 1000;

QLabel *makeCaption(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("caption"));
    return label;
}

quint16 readU16(const QByteArray &data, int offset)
{
    return (static_cast<quint8>(data.at(offset)) << 8)
           | static_cast<quint8>(data.at(offset + 1));
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_serial(new QSerialPort(this)),
      m_tcpSocket(new QTcpSocket(this)),
      m_tcpServer(new QTcpServer(this)),
      m_responseTimer(new QTimer(this)),
      m_frameGapTimer(new QTimer(this)),
      m_pollTimer(new QTimer(this)),
      m_coils(65536, 0),
      m_discreteInputs(65536, 0),
      m_holdingRegisters(65536, 0),
      m_inputRegisters(65536, 0)
{
    buildUi();
    applyStyle();

    m_responseTimer->setSingleShot(true);
    m_frameGapTimer->setSingleShot(true);
    m_frameGapTimer->setInterval(40);

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::sendRequest);
    connect(m_rawSendButton, &QPushButton::clicked, this, &MainWindow::sendRawFrame);
    connect(m_rawEdit, &QLineEdit::returnPressed, this, &MainWindow::sendRawFrame);
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::handleSerialError);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::readTcpData);
    connect(m_tcpSocket, &QTcpSocket::connected, this, [this] {
        setConnectedState(true);
        setStatus(QStringLiteral("已连接到 %1:%2")
                      .arg(m_hostEdit->text()).arg(m_tcpPortSpin->value()));
        appendLog(QStringLiteral("SYS"), {}, QStringLiteral("Modbus TCP 已连接"));
    });
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, [this] {
        m_responseTimer->stop();
        m_waitingForResponse = false;
        setConnectedState(false);
        setStatus(QStringLiteral("TCP 连接已断开"));
    });
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        if (m_tcpSocket->state() != QAbstractSocket::ConnectedState)
            setConnectedState(false);
        setStatus(QStringLiteral("TCP 错误：%1").arg(m_tcpSocket->errorString()), true);
    });
    connect(m_tcpServer, &QTcpServer::newConnection, this, &MainWindow::acceptTcpClient);
    connect(m_responseTimer, &QTimer::timeout, this, &MainWindow::handleResponseTimeout);
    connect(m_frameGapTimer, &QTimer::timeout, this, &MainWindow::processReceivedFrame);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::sendRequest);
    connect(m_functionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateFunctionUi);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateModeUi);
    connect(m_protocolCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateProtocolUi);
    connect(m_pollCheck, &QCheckBox::toggled, this, &MainWindow::togglePolling);
    connect(m_pollIntervalSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int interval) { m_pollTimer->setInterval(interval); });
    connect(m_slaveAreaCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshSlaveDataTable);
    connect(m_slaveViewStartSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSlaveDataTable);
    connect(m_slaveViewCountSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSlaveDataTable);
    connect(m_slaveRefreshButton, &QPushButton::clicked,
            this, &MainWindow::refreshSlaveDataTable);
    connect(m_resultTable, &QTableWidget::cellChanged,
            this, &MainWindow::updateSlaveDataFromTable);

    loadSettings();
    refreshPorts();
    updateFunctionUi();
    updateProtocolUi();
    updateModeUi();
    setConnectedState(false);
    setStatus(QStringLiteral("就绪，请选择串口并打开"));
}

MainWindow::~MainWindow()
{
    if (m_serial->isOpen())
        m_serial->close();
    m_tcpSocket->abort();
    m_tcpServer->close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Modbus 通讯助手"));
    resize(1180, 760);
    setMinimumSize(980, 650);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(16);

    auto *headingRow = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("Modbus 通讯助手"));
    title->setObjectName(QStringLiteral("title"));
    auto *subtitle = new QLabel(QStringLiteral("RTU · ASCII · TCP · 主站/从站"));
    subtitle->setObjectName(QStringLiteral("subtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    headingRow->addLayout(titles);
    headingRow->addStretch();
    m_connectionBadge = new QLabel(QStringLiteral("RTU · 主站 · 未连接"));
    m_connectionBadge->setObjectName(QStringLiteral("connectionBadge"));
    headingRow->addWidget(m_connectionBadge);
    root->addLayout(headingRow);

    auto *serialGroup = new QGroupBox(QStringLiteral("通讯设置"));
    auto *serialLayout = new QGridLayout(serialGroup);
    serialLayout->setContentsMargins(16, 20, 16, 14);
    serialLayout->setHorizontalSpacing(12);
    serialLayout->setVerticalSpacing(8);

    m_protocolCombo = new QComboBox;
    m_protocolCombo->addItem(QStringLiteral("Modbus RTU"), 0);
    m_protocolCombo->addItem(QStringLiteral("Modbus ASCII"), 1);
    m_protocolCombo->addItem(QStringLiteral("Modbus TCP"), 2);
    m_protocolCombo->setMinimumWidth(125);
    m_protocolCombo->view()->setMinimumWidth(180);
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(QStringLiteral("主站"), 0);
    m_modeCombo->addItem(QStringLiteral("从站"), 1);
    m_modeCombo->setMinimumWidth(84);
    m_portCombo = new QComboBox;
    m_portCombo->setMinimumWidth(160);
    m_portCombo->setMinimumContentsLength(12);
    m_portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_portCombo->view()->setMinimumWidth(360);
    m_refreshButton = new QPushButton(QStringLiteral("刷新"));
    m_refreshButton->setObjectName(QStringLiteral("secondaryButton"));
    m_baudCombo = new QComboBox;
    m_baudCombo->setMinimumWidth(96);
    for (int baud : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200})
        m_baudCombo->addItem(QString::number(baud), baud);
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->setMinimumWidth(72);
    m_dataBitsCombo->addItem(QStringLiteral("8"), QSerialPort::Data8);
    m_dataBitsCombo->addItem(QStringLiteral("7"), QSerialPort::Data7);
    m_parityCombo = new QComboBox;
    m_parityCombo->setMinimumWidth(100);
    m_parityCombo->addItem(QStringLiteral("无校验"), QSerialPort::NoParity);
    m_parityCombo->addItem(QStringLiteral("偶校验"), QSerialPort::EvenParity);
    m_parityCombo->addItem(QStringLiteral("奇校验"), QSerialPort::OddParity);
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->setMinimumWidth(72);
    m_stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    m_stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
    m_flowControlCombo = new QComboBox;
    m_flowControlCombo->setMinimumWidth(90);
    m_flowControlCombo->addItem(QStringLiteral("无"), QSerialPort::NoFlowControl);
    m_flowControlCombo->addItem(QStringLiteral("硬件"), QSerialPort::HardwareControl);
    m_flowControlCombo->addItem(QStringLiteral("软件"), QSerialPort::SoftwareControl);
    const QList<QComboBox *> communicationCombos{
        m_protocolCombo, m_modeCombo, m_portCombo, m_baudCombo,
        m_dataBitsCombo, m_parityCombo, m_stopBitsCombo, m_flowControlCombo
    };
    for (QComboBox *combo : communicationCombos) {
        combo->setMaxVisibleItems(20);
        combo->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        combo->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_hostEdit->setMinimumWidth(180);
    m_tcpPortSpin = new QSpinBox;
    m_tcpPortSpin->setRange(1, 65535);
    m_tcpPortSpin->setValue(502);
    m_tcpPortSpin->setMinimumWidth(100);
    m_openButton = new QPushButton(QStringLiteral("打开串口"));
    m_openButton->setObjectName(QStringLiteral("primaryButton"));
    m_openButton->setMinimumWidth(108);
    m_openButton->setMinimumHeight(36);

    auto makeCommunicationField = [&](const QString &caption, QWidget *field) -> QWidget * {
        auto *container = new QWidget(serialGroup);
        container->setObjectName(QStringLiteral("fieldContainer"));
        auto *column = new QVBoxLayout(container);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);
        column->addWidget(makeCaption(caption));
        column->addWidget(field);
        return container;
    };

    auto *protocolField = makeCommunicationField(QStringLiteral("协议"), m_protocolCombo);
    auto *modeField = makeCommunicationField(QStringLiteral("工作模式"), m_modeCombo);
    auto *portField = makeCommunicationField(QStringLiteral("端口"), m_portCombo);
    auto *baudField = makeCommunicationField(QStringLiteral("波特率"), m_baudCombo);
    auto *dataBitsField = makeCommunicationField(QStringLiteral("数据位"), m_dataBitsCombo);
    auto *parityField = makeCommunicationField(QStringLiteral("校验位"), m_parityCombo);
    auto *stopBitsField = makeCommunicationField(QStringLiteral("停止位"), m_stopBitsCombo);
    auto *flowField = makeCommunicationField(QStringLiteral("流控"), m_flowControlCombo);
    auto *hostField = makeCommunicationField(QStringLiteral("IP 地址 / 主机名"), m_hostEdit);
    auto *tcpPortField = makeCommunicationField(QStringLiteral("TCP 端口"), m_tcpPortSpin);

    serialLayout->addWidget(protocolField, 0, 0, 2, 1, Qt::AlignTop);
    serialLayout->addWidget(modeField, 0, 1, 2, 1, Qt::AlignTop);
    serialLayout->addWidget(portField, 0, 2, 1, 2);
    serialLayout->addWidget(m_refreshButton, 0, 4, 1, 1, Qt::AlignBottom);
    serialLayout->addWidget(baudField, 0, 5);
    serialLayout->addWidget(dataBitsField, 1, 2);
    serialLayout->addWidget(parityField, 1, 3);
    serialLayout->addWidget(stopBitsField, 1, 4);
    serialLayout->addWidget(flowField, 1, 5);
    serialLayout->addWidget(hostField, 0, 2, 1, 3);
    serialLayout->addWidget(tcpPortField, 0, 5);
    serialLayout->addWidget(m_openButton, 0, 6, 2, 1, Qt::AlignVCenter);
    serialLayout->setColumnStretch(2, 2);
    serialLayout->setColumnStretch(3, 1);
    serialLayout->setColumnStretch(5, 1);
    serialLayout->setColumnMinimumWidth(6, 108);

    m_serialFieldContainers << portField;
    m_serialFieldContainers << m_refreshButton;
    m_serialFieldContainers << baudField << dataBitsField << parityField
                            << stopBitsField << flowField;
    m_tcpFieldContainers << hostField << tcpPortField;
    root->addWidget(serialGroup);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);

    auto *requestPanel = new QFrame;
    requestPanel->setObjectName(QStringLiteral("panel"));
    requestPanel->setMinimumWidth(350);
    requestPanel->setMaximumWidth(430);
    auto *requestLayout = new QVBoxLayout(requestPanel);
    requestLayout->setContentsMargins(16, 14, 16, 16);
    requestLayout->setSpacing(11);
    m_requestTitle = new QLabel(QStringLiteral("主站请求"));
    m_requestTitle->setObjectName(QStringLiteral("sectionTitle"));
    requestLayout->addWidget(m_requestTitle);

    m_controlStack = new QStackedWidget;
    m_masterControls = new QWidget;
    auto *masterLayout = new QVBoxLayout(m_masterControls);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(11);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    m_slaveSpin = new QSpinBox;
    m_slaveSpin->setRange(1, 247);
    m_slaveSpin->setValue(1);
    m_functionCombo = new QComboBox;
    m_functionCombo->addItem(QStringLiteral("01 - 读线圈"), 0x01);
    m_functionCombo->addItem(QStringLiteral("02 - 读离散输入"), 0x02);
    m_functionCombo->addItem(QStringLiteral("03 - 读保持寄存器"), 0x03);
    m_functionCombo->addItem(QStringLiteral("04 - 读输入寄存器"), 0x04);
    m_functionCombo->addItem(QStringLiteral("05 - 写单个线圈"), 0x05);
    m_functionCombo->addItem(QStringLiteral("06 - 写单个寄存器"), 0x06);
    m_functionCombo->addItem(QStringLiteral("0F - 写多个线圈"), 0x0F);
    m_functionCombo->addItem(QStringLiteral("10 - 写多个寄存器"), 0x10);
    m_addressSpin = new QSpinBox;
    m_addressSpin->setRange(0, 65535);
    m_addressSpin->setDisplayIntegerBase(10);
    m_quantitySpin = new QSpinBox;
    m_quantitySpin->setRange(1, 125);
    m_quantitySpin->setValue(10);
    m_writeDataEdit = new QLineEdit;
    m_writeDataEdit->setClearButtonEnabled(true);
    m_dataLabel = new QLabel(QStringLiteral("写入数据"));

    form->addRow(QStringLiteral("从站地址"), m_slaveSpin);
    form->addRow(QStringLiteral("功能码"), m_functionCombo);
    form->addRow(QStringLiteral("起始地址"), m_addressSpin);
    form->addRow(QStringLiteral("数量"), m_quantitySpin);
    form->addRow(m_dataLabel, m_writeDataEdit);
    masterLayout->addLayout(form);

    auto *hint = new QLabel(QStringLiteral("提示：地址按协议从 0 开始，支持十进制和 0x 十六进制写入值。"));
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    masterLayout->addWidget(hint);

    auto *timingBox = new QGroupBox(QStringLiteral("通讯控制"));
    timingBox->setObjectName(QStringLiteral("timingBox"));
    auto *timingLayout = new QFormLayout(timingBox);
    timingLayout->setContentsMargins(12, 16, 12, 10);
    timingLayout->setHorizontalSpacing(12);
    timingLayout->setVerticalSpacing(10);
    m_timeoutSpin = new QSpinBox;
    m_timeoutSpin->setRange(100, 30000);
    m_timeoutSpin->setValue(kDefaultTimeoutMs);
    m_timeoutSpin->setSuffix(QStringLiteral(" ms"));
    m_pollCheck = new QCheckBox(QStringLiteral("启用定时轮询"));
    m_pollIntervalSpin = new QSpinBox;
    m_pollIntervalSpin->setRange(100, 60000);
    m_pollIntervalSpin->setValue(1000);
    m_pollIntervalSpin->setSuffix(QStringLiteral(" ms"));
    timingLayout->addRow(QStringLiteral("响应超时"), m_timeoutSpin);
    timingLayout->addRow(m_pollCheck);
    timingLayout->addRow(QStringLiteral("轮询间隔"), m_pollIntervalSpin);
    masterLayout->addWidget(timingBox);
    masterLayout->addStretch();
    m_controlStack->addWidget(m_masterControls);

    m_slaveControls = new QWidget;
    auto *slaveLayout = new QVBoxLayout(m_slaveControls);
    slaveLayout->setContentsMargins(0, 0, 0, 0);
    slaveLayout->setSpacing(12);
    auto *slaveHelp = new QLabel(QStringLiteral(
        "从站模式会监听主站请求并自动响应。可在右侧数据表中双击“当前值”进行修改。"));
    slaveHelp->setObjectName(QStringLiteral("hint"));
    slaveHelp->setWordWrap(true);
    slaveLayout->addWidget(slaveHelp);

    auto *slaveForm = new QFormLayout;
    slaveForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    slaveForm->setHorizontalSpacing(12);
    slaveForm->setVerticalSpacing(10);
    m_slaveUnitSpin = new QSpinBox;
    m_slaveUnitSpin->setRange(1, 247);
    m_slaveUnitSpin->setValue(1);
    m_slaveAreaCombo = new QComboBox;
    m_slaveAreaCombo->addItem(QStringLiteral("线圈 (0x)"), 0);
    m_slaveAreaCombo->addItem(QStringLiteral("离散输入 (1x)"), 1);
    m_slaveAreaCombo->addItem(QStringLiteral("保持寄存器 (4x)"), 2);
    m_slaveAreaCombo->addItem(QStringLiteral("输入寄存器 (3x)"), 3);
    m_slaveViewStartSpin = new QSpinBox;
    m_slaveViewStartSpin->setRange(0, 65535);
    m_slaveViewCountSpin = new QSpinBox;
    m_slaveViewCountSpin->setRange(1, 100);
    m_slaveViewCountSpin->setValue(20);
    slaveForm->addRow(QStringLiteral("本站地址"), m_slaveUnitSpin);
    slaveForm->addRow(QStringLiteral("数据区"), m_slaveAreaCombo);
    slaveForm->addRow(QStringLiteral("显示起始地址"), m_slaveViewStartSpin);
    slaveForm->addRow(QStringLiteral("显示数量"), m_slaveViewCountSpin);
    slaveLayout->addLayout(slaveForm);

    m_slaveRefreshButton = new QPushButton(QStringLiteral("刷新从站数据"));
    m_slaveRefreshButton->setObjectName(QStringLiteral("secondaryButton"));
    slaveLayout->addWidget(m_slaveRefreshButton);
    auto *slaveNote = new QLabel(QStringLiteral(
        "支持 01、02、03、04、05、06、0F、10 功能码；收到写入请求后数据表会自动更新。"));
    slaveNote->setObjectName(QStringLiteral("hint"));
    slaveNote->setWordWrap(true);
    slaveLayout->addWidget(slaveNote);
    slaveLayout->addStretch();
    m_controlStack->addWidget(m_slaveControls);

    requestLayout->addWidget(m_controlStack, 1);

    m_sendButton = new QPushButton(QStringLiteral("发送请求"));
    m_sendButton->setObjectName(QStringLiteral("accentButton"));
    m_sendButton->setMinimumHeight(38);
    requestLayout->addWidget(m_sendButton);
    splitter->addWidget(requestPanel);

    auto *tabs = new QTabWidget;
    tabs->setDocumentMode(true);

    auto *resultPage = new QWidget;
    auto *resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(10, 12, 10, 10);
    auto *resultHint = new QLabel(QStringLiteral("解析结果"));
    resultHint->setObjectName(QStringLiteral("sectionTitle"));
    resultLayout->addWidget(resultHint);
    m_resultTable = new QTableWidget(0, 4);
    m_resultTable->setHorizontalHeaderLabels(
        {QStringLiteral("地址"), QStringLiteral("十进制"), QStringLiteral("十六进制"),
         QStringLiteral("二进制 / 状态")});
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->verticalHeader()->setDefaultSectionSize(34);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultLayout->addWidget(m_resultTable);
    tabs->addTab(resultPage, QStringLiteral("数据解析"));

    auto *logPage = new QWidget;
    auto *logLayout = new QVBoxLayout(logPage);
    logLayout->setContentsMargins(10, 10, 10, 10);
    auto *logTools = new QHBoxLayout;
    logTools->addWidget(new QLabel(QStringLiteral("通讯报文")));
    logTools->addStretch();
    auto *clearButton = new QPushButton(QStringLiteral("清空"));
    auto *saveButton = new QPushButton(QStringLiteral("保存日志"));
    clearButton->setObjectName(QStringLiteral("secondaryButton"));
    saveButton->setObjectName(QStringLiteral("secondaryButton"));
    logTools->addWidget(clearButton);
    logTools->addWidget(saveButton);
    logLayout->addLayout(logTools);
    m_logEdit = new QPlainTextEdit;
    m_logEdit->setObjectName(QStringLiteral("logConsole"));
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(5000);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logEdit->setPlaceholderText(QStringLiteral("打开串口后，收发报文会显示在这里…"));
    QFont logFont(QStringLiteral("Cascadia Mono"));
    logFont.setStyleHint(QFont::Monospace);
    logFont.setPointSize(9);
    m_logEdit->setFont(logFont);
    logLayout->addWidget(m_logEdit);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveLog);
    tabs->addTab(logPage, QStringLiteral("通讯日志"));

    auto *rawPage = new QWidget;
    auto *rawLayout = new QVBoxLayout(rawPage);
    rawLayout->setContentsMargins(16, 16, 16, 16);
    auto *rawTitle = new QLabel(QStringLiteral("发送自定义 Modbus 请求"));
    rawTitle->setObjectName(QStringLiteral("sectionTitle"));
    rawLayout->addWidget(rawTitle);
    auto *rawHelp = new QLabel(QStringLiteral(
        "输入从站地址和 PDU 的十六进制字节，例如：01 03 00 00 00 0A。程序会按当前协议封装报文。"));
    rawHelp->setObjectName(QStringLiteral("hint"));
    rawHelp->setWordWrap(true);
    rawLayout->addWidget(rawHelp);
    m_rawEdit = new QLineEdit;
    m_rawEdit->setPlaceholderText(QStringLiteral("01 03 00 00 00 0A"));
    m_rawEdit->setClearButtonEnabled(true);
    QFont rawFont(QStringLiteral("Cascadia Mono"));
    rawFont.setStyleHint(QFont::Monospace);
    m_rawEdit->setFont(rawFont);
    rawLayout->addWidget(m_rawEdit);
    auto *rawActions = new QHBoxLayout;
    m_autoCrcCheck = new QCheckBox(QStringLiteral("自动追加 Modbus CRC16"));
    m_autoCrcCheck->setChecked(true);
    m_rawSendButton = new QPushButton(QStringLiteral("发送原始报文"));
    m_rawSendButton->setObjectName(QStringLiteral("accentButton"));
    rawActions->addWidget(m_autoCrcCheck);
    rawActions->addStretch();
    rawActions->addWidget(m_rawSendButton);
    rawLayout->addLayout(rawActions);
    rawLayout->addStretch();
    tabs->addTab(rawPage, QStringLiteral("原始报文"));

    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({355, 760});
    root->addWidget(splitter, 1);

    auto *bottomBar = new QFrame;
    bottomBar->setObjectName(QStringLiteral("bottomBar"));
    auto *bottom = new QHBoxLayout(bottomBar);
    bottom->setContentsMargins(12, 8, 12, 8);
    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName(QStringLiteral("statusText"));
    m_counterLabel = new QLabel(QStringLiteral("TX 0  ·  RX 0"));
    m_counterLabel->setObjectName(QStringLiteral("counterText"));
    bottom->addWidget(m_statusLabel);
    bottom->addStretch();
    bottom->addWidget(m_counterLabel);
    root->addWidget(bottomBar);

    setCentralWidget(central);

    m_protocolCombo->setAccessibleName(QStringLiteral("Modbus 协议"));
    m_modeCombo->setAccessibleName(QStringLiteral("主站或从站模式"));
    m_portCombo->setAccessibleName(QStringLiteral("串口端口"));
    m_openButton->setAccessibleName(QStringLiteral("打开或关闭通讯连接"));
    m_sendButton->setAccessibleName(QStringLiteral("发送 Modbus 请求"));
    m_resultTable->setAccessibleName(QStringLiteral("Modbus 数据解析结果"));
    m_logEdit->setAccessibleName(QStringLiteral("通讯收发日志"));
    m_rawEdit->setAccessibleName(QStringLiteral("自定义十六进制请求"));
    m_protocolCombo->setToolTip(QStringLiteral("选择 RTU、ASCII 或 TCP 通讯协议"));
    m_modeCombo->setToolTip(QStringLiteral("主站主动请求；从站监听并响应"));
    m_resultTable->setToolTip(QStringLiteral("从站模式下可双击当前值进行编辑"));

    auto *refreshShortcut = new QShortcut(QKeySequence::Refresh, this);
    connect(refreshShortcut, &QShortcut::activated, this, &MainWindow::refreshPorts);
    auto *sendShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(sendShortcut, &QShortcut::activated, this, &MainWindow::sendRequest);
}

void MainWindow::applyStyle()
{
    QFile themeFile(QStringLiteral(":/themes/industrial.qss"));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(themeFile.readAll()));
}

void MainWindow::refreshPorts()
{
    const QString selected = m_portCombo->currentData().toString();
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        QString label = port.portName();
        if (!port.description().isEmpty())
            label += QStringLiteral(" — ") + port.description();
        m_portCombo->addItem(label, port.portName());
        const int index = m_portCombo->count() - 1;
        m_portCombo->setItemData(index,
            QStringLiteral("%1\n%2").arg(port.manufacturer(), port.systemLocation()),
            Qt::ToolTipRole);
    }

    const int oldIndex = m_portCombo->findData(selected);
    if (oldIndex >= 0)
        m_portCombo->setCurrentIndex(oldIndex);
    if (ports.isEmpty())
        setStatus(QStringLiteral("未发现可用串口，请连接设备后刷新"), true);
    else
        setStatus(QStringLiteral("发现 %1 个串口").arg(ports.size()));
}

void MainWindow::toggleSerialPort()
{
    if (isTcpMode()) {
        if (isSlaveMode()) {
            if (m_tcpServer->isListening()) {
                if (m_slaveTcpClient)
                    m_slaveTcpClient->disconnectFromHost();
                m_tcpServer->close();
                m_tcpReceiveBuffer.clear();
                setConnectedState(false);
                setStatus(QStringLiteral("TCP 从站监听已停止"));
                return;
            }
            QHostAddress listenAddress;
            const QString host = m_hostEdit->text().trimmed();
            if (host.isEmpty() || host == QStringLiteral("0.0.0.0")
                || host == QStringLiteral("*")) {
                listenAddress = QHostAddress::AnyIPv4;
            } else if (!listenAddress.setAddress(host)) {
                setStatus(QStringLiteral("监听 IP 地址格式不正确"), true);
                return;
            }
            if (!m_tcpServer->listen(listenAddress,
                                     static_cast<quint16>(m_tcpPortSpin->value()))) {
                setStatus(QStringLiteral("TCP 监听失败：%1").arg(m_tcpServer->errorString()), true);
                return;
            }
            setConnectedState(true);
            setStatus(QStringLiteral("TCP 从站 %1 正在监听 %2:%3")
                          .arg(m_slaveUnitSpin->value())
                          .arg(m_tcpServer->serverAddress().toString())
                          .arg(m_tcpServer->serverPort()));
            appendLog(QStringLiteral("SYS"), {}, QStringLiteral("Modbus TCP 从站监听已启动"));
        } else {
            if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
                m_tcpSocket->abort();
                setConnectedState(false);
                setStatus(QStringLiteral("TCP 连接已断开"));
                return;
            }
            const QString host = m_hostEdit->text().trimmed();
            if (host.isEmpty()) {
                setStatus(QStringLiteral("请输入服务器 IP 地址或主机名"), true);
                return;
            }
            setStatus(QStringLiteral("正在连接 %1:%2…").arg(host).arg(m_tcpPortSpin->value()));
            m_openButton->setEnabled(false);
            m_tcpSocket->connectToHost(host, static_cast<quint16>(m_tcpPortSpin->value()));
            QTimer::singleShot(5000, this, [this] {
                if (m_tcpSocket->state() == QAbstractSocket::ConnectingState) {
                    m_tcpSocket->abort();
                    m_openButton->setEnabled(true);
                    setConnectedState(false);
                    setStatus(QStringLiteral("TCP 连接超时"), true);
                }
            });
        }
        return;
    }

    if (m_serial->isOpen()) {
        m_pollCheck->setChecked(false);
        m_responseTimer->stop();
        m_frameGapTimer->stop();
        m_serial->close();
        m_receiveBuffer.clear();
        m_waitingForResponse = false;
        setConnectedState(false);
        setStatus(QStringLiteral("串口已关闭"));
        return;
    }

    if (m_portCombo->currentIndex() < 0) {
        setStatus(QStringLiteral("没有可打开的串口"), true);
        return;
    }

    m_serial->setPortName(m_portCombo->currentData().toString());
    m_serial->setBaudRate(m_baudCombo->currentData().toInt());
    m_serial->setDataBits(static_cast<QSerialPort::DataBits>(m_dataBitsCombo->currentData().toInt()));
    m_serial->setParity(static_cast<QSerialPort::Parity>(m_parityCombo->currentData().toInt()));
    m_serial->setStopBits(static_cast<QSerialPort::StopBits>(m_stopBitsCombo->currentData().toInt()));
    m_serial->setFlowControl(
        static_cast<QSerialPort::FlowControl>(m_flowControlCombo->currentData().toInt()));

    if (!m_serial->open(QIODevice::ReadWrite)) {
        setStatus(QStringLiteral("打开失败：%1").arg(m_serial->errorString()), true);
        QMessageBox::warning(this, QStringLiteral("无法打开串口"), m_serial->errorString());
        return;
    }

    setConnectedState(true);
    if (isSlaveMode()) {
        setStatus(QStringLiteral("%1 已打开，从站 %2 正在监听")
                      .arg(m_serial->portName())
                      .arg(m_slaveUnitSpin->value()));
    } else {
        setStatus(QStringLiteral("%1 已打开，%2 bps")
                      .arg(m_serial->portName())
                      .arg(m_serial->baudRate()));
    }
    appendLog(QStringLiteral("SYS"), {},
              QStringLiteral("串口已打开：%1 @ %2，%3模式")
                  .arg(m_serial->portName()).arg(m_serial->baudRate())
                  .arg(isSlaveMode() ? QStringLiteral("从站") : QStringLiteral("主站")));
}

void MainWindow::setConnectedState(bool connected)
{
    m_openButton->setEnabled(true);
    if (isTcpMode()) {
        m_openButton->setText(connected
            ? (isSlaveMode() ? QStringLiteral("停止监听") : QStringLiteral("断开连接"))
            : (isSlaveMode() ? QStringLiteral("开始监听") : QStringLiteral("连接")));
    } else {
        m_openButton->setText(connected ? QStringLiteral("关闭串口") : QStringLiteral("打开串口"));
    }
    m_openButton->setProperty("connected", connected);
    QString protocolName = m_protocolCombo->currentText();
    protocolName.remove(QStringLiteral("Modbus "));
    const QString modeName = isSlaveMode() ? QStringLiteral("从站") : QStringLiteral("主站");
    const QString connectionState = connected
        ? (isTcpMode() && isSlaveMode() ? QStringLiteral("监听中") : QStringLiteral("已连接"))
        : QStringLiteral("未连接");
    m_connectionBadge->setText(
        QStringLiteral("%1 · %2 · %3").arg(protocolName, modeName, connectionState));
    m_connectionBadge->setProperty("connected", connected);

    const QList<QWidget *> configurationWidgets{
        m_protocolCombo, m_modeCombo, m_portCombo, m_refreshButton, m_baudCombo, m_dataBitsCombo,
        m_parityCombo, m_stopBitsCombo, m_flowControlCombo
    };
    for (QWidget *widget : configurationWidgets) {
        widget->setEnabled(!connected);
    }
    m_hostEdit->setEnabled(!connected);
    m_tcpPortSpin->setEnabled(!connected);
    m_sendButton->setEnabled(connected && !isSlaveMode());
    m_rawSendButton->setEnabled(connected && !isSlaveMode());
    m_pollCheck->setEnabled(connected && !isSlaveMode());
    m_slaveUnitSpin->setEnabled(!connected);

    m_openButton->style()->unpolish(m_openButton);
    m_openButton->style()->polish(m_openButton);
    m_connectionBadge->style()->unpolish(m_connectionBadge);
    m_connectionBadge->style()->polish(m_connectionBadge);
}

void MainWindow::setBusyState(bool busy)
{
    m_sendButton->setEnabled(isTransportOpen() && !busy && !isSlaveMode());
    m_rawSendButton->setEnabled(isTransportOpen() && !busy && !isSlaveMode());
    m_sendButton->setText(busy ? QStringLiteral("等待响应…") : QStringLiteral("发送请求"));
    m_sendButton->setProperty("busy", busy);
    m_sendButton->style()->unpolish(m_sendButton);
    m_sendButton->style()->polish(m_sendButton);
}

void MainWindow::updateFunctionUi()
{
    const int function = m_functionCombo->currentData().toInt();
    const bool writes = function == 0x05 || function == 0x06
                        || function == 0x0F || function == 0x10;
    m_dataLabel->setVisible(writes);
    m_writeDataEdit->setVisible(writes);

    if (function == 0x01 || function == 0x02) {
        m_quantitySpin->setEnabled(true);
        m_quantitySpin->setRange(1, 2000);
    } else if (function == 0x03 || function == 0x04) {
        m_quantitySpin->setEnabled(true);
        m_quantitySpin->setRange(1, 125);
    } else if (function == 0x05) {
        m_quantitySpin->setRange(1, 1);
        m_quantitySpin->setValue(1);
        m_quantitySpin->setEnabled(false);
        m_writeDataEdit->setPlaceholderText(QStringLiteral("0 或 1"));
    } else if (function == 0x06) {
        m_quantitySpin->setRange(1, 1);
        m_quantitySpin->setValue(1);
        m_quantitySpin->setEnabled(false);
        m_writeDataEdit->setPlaceholderText(QStringLiteral("例如 1234 或 0x04D2"));
    } else if (function == 0x0F) {
        m_quantitySpin->setEnabled(true);
        m_quantitySpin->setRange(1, 1968);
        m_writeDataEdit->setPlaceholderText(QStringLiteral("例如 1, 0, 1, 1"));
    } else if (function == 0x10) {
        m_quantitySpin->setEnabled(true);
        m_quantitySpin->setRange(1, 123);
        m_writeDataEdit->setPlaceholderText(QStringLiteral("例如 100, 200, 0x1234"));
    }
}

bool MainWindow::isSlaveMode() const
{
    return m_modeCombo && m_modeCombo->currentData().toInt() == 1;
}

bool MainWindow::isTcpMode() const
{
    return m_protocolCombo && m_protocolCombo->currentData().toInt() == 2;
}

bool MainWindow::isAsciiMode() const
{
    return m_protocolCombo && m_protocolCombo->currentData().toInt() == 1;
}

bool MainWindow::isTransportOpen() const
{
    if (isTcpMode()) {
        return isSlaveMode() ? m_tcpServer->isListening()
                             : m_tcpSocket->state() == QAbstractSocket::ConnectedState;
    }
    return m_serial->isOpen();
}

void MainWindow::updateProtocolUi()
{
    const bool tcp = isTcpMode();
    for (QWidget *widget : m_serialFieldContainers)
        widget->setVisible(!tcp);
    for (QWidget *widget : m_tcpFieldContainers)
        widget->setVisible(tcp);

    m_rawSendButton->setText(tcp ? QStringLiteral("发送 TCP 请求")
                                 : isAsciiMode() ? QStringLiteral("发送 ASCII 请求")
                                                 : QStringLiteral("发送原始报文"));
    if (tcp) {
        m_autoCrcCheck->setText(QStringLiteral("自动生成 MBAP 事务头"));
        m_autoCrcCheck->setChecked(true);
        m_autoCrcCheck->setEnabled(false);
    } else if (isAsciiMode()) {
        m_autoCrcCheck->setText(QStringLiteral("自动生成 ASCII LRC"));
        m_autoCrcCheck->setChecked(true);
        m_autoCrcCheck->setEnabled(false);
    } else {
        m_autoCrcCheck->setText(QStringLiteral("自动追加 Modbus CRC16"));
        m_autoCrcCheck->setEnabled(true);
    }
    setConnectedState(isTransportOpen());
    if (tcp) {
        setStatus(QStringLiteral("已选择 Modbus TCP，请设置 IP 地址和端口"));
    } else if (isAsciiMode()) {
        setStatus(QStringLiteral("已选择 Modbus ASCII，使用串口传输和 LRC 校验"));
    } else {
        setStatus(QStringLiteral("已选择 Modbus RTU，使用串口传输和 CRC16 校验"));
    }
}

void MainWindow::updateModeUi()
{
    const bool slaveMode = isSlaveMode();
    m_controlStack->setCurrentWidget(slaveMode ? m_slaveControls : m_masterControls);
    m_requestTitle->setText(slaveMode ? QStringLiteral("从站仿真") : QStringLiteral("主站请求"));
    m_sendButton->setVisible(!slaveMode);
    m_pollCheck->setChecked(false);

    if (slaveMode) {
        m_resultTable->setEditTriggers(
            QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        refreshSlaveDataTable();
        setStatus(QStringLiteral("已切换到从站模式，打开串口后开始监听"));
    } else {
        m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_resultTable->setRowCount(0);
        m_resultTable->setHorizontalHeaderLabels(
            {QStringLiteral("地址"), QStringLiteral("十进制"), QStringLiteral("十六进制"),
             QStringLiteral("二进制 / 状态")});
        setStatus(QStringLiteral("已切换到主站模式"));
    }
    setConnectedState(isTransportOpen());
}

void MainWindow::refreshSlaveDataTable()
{
    if (!isSlaveMode())
        return;

    const int area = m_slaveAreaCombo->currentData().toInt();
    const int start = m_slaveViewStartSpin->value();
    const int count = qMin(m_slaveViewCountSpin->value(), 65536 - start);
    const bool bitArea = area == 0 || area == 1;
    const QString typeName = m_slaveAreaCombo->currentText();

    m_resultTable->blockSignals(true);
    m_resultTable->clearContents();
    m_resultTable->setRowCount(count);
    m_resultTable->setHorizontalHeaderLabels(
        {QStringLiteral("地址"), QStringLiteral("当前值（可编辑）"),
         QStringLiteral("十六进制"), QStringLiteral("数据区 / 状态")});

    for (int row = 0; row < count; ++row) {
        const int address = start + row;
        quint16 value = 0;
        if (area == 0)
            value = m_coils.at(address);
        else if (area == 1)
            value = m_discreteInputs.at(address);
        else if (area == 2)
            value = m_holdingRegisters.at(address);
        else
            value = m_inputRegisters.at(address);

        auto *addressItem = new QTableWidgetItem(QString::number(address));
        addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
        addressItem->setData(Qt::UserRole, address);
        auto *valueItem = new QTableWidgetItem(QString::number(value));
        valueItem->setData(Qt::UserRole, address);
        auto *hexItem = new QTableWidgetItem(
            QStringLiteral("0x%1").arg(value, bitArea ? 2 : 4, 16, QLatin1Char('0')).toUpper());
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        auto *stateItem = new QTableWidgetItem(
            bitArea ? (value ? QStringLiteral("ON / 接通") : QStringLiteral("OFF / 断开"))
                    : typeName);
        stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable);

        m_resultTable->setItem(row, 0, addressItem);
        m_resultTable->setItem(row, 1, valueItem);
        m_resultTable->setItem(row, 2, hexItem);
        m_resultTable->setItem(row, 3, stateItem);
    }
    m_resultTable->blockSignals(false);
}

void MainWindow::updateSlaveDataFromTable(int row, int column)
{
    if (!isSlaveMode() || column != 1 || row < 0)
        return;

    QTableWidgetItem *item = m_resultTable->item(row, column);
    if (!item)
        return;
    const int address = item->data(Qt::UserRole).toInt();
    const int area = m_slaveAreaCombo->currentData().toInt();
    const bool bitArea = area == 0 || area == 1;
    const QString text = item->text().trimmed();
    bool ok = false;
    const uint value = text.toUInt(&ok, text.startsWith(QStringLiteral("0x"),
                                                        Qt::CaseInsensitive) ? 16 : 10);
    const uint maximum = bitArea ? 1u : 65535u;
    if (!ok || value > maximum) {
        setStatus(bitArea ? QStringLiteral("位数据只能输入 0 或 1")
                          : QStringLiteral("寄存器数据必须在 0 到 65535 之间"), true);
        refreshSlaveDataTable();
        return;
    }

    if (area == 0)
        m_coils[address] = static_cast<quint8>(value);
    else if (area == 1)
        m_discreteInputs[address] = static_cast<quint8>(value);
    else if (area == 2)
        m_holdingRegisters[address] = static_cast<quint16>(value);
    else
        m_inputRegisters[address] = static_cast<quint16>(value);

    refreshSlaveDataTable();
    setStatus(QStringLiteral("从站数据已更新：地址 %1 = %2").arg(address).arg(value));
}

QByteArray MainWindow::buildRequest(QString *errorMessage) const
{
    const int slave = m_slaveSpin->value();
    const int function = m_functionCombo->currentData().toInt();
    const int address = m_addressSpin->value();
    const int quantity = m_quantitySpin->value();

    QByteArray frame;
    frame.append(static_cast<char>(slave));
    frame.append(static_cast<char>(function));
    frame.append(static_cast<char>((address >> 8) & 0xFF));
    frame.append(static_cast<char>(address & 0xFF));

    auto appendWord = [&frame](quint16 value) {
        frame.append(static_cast<char>((value >> 8) & 0xFF));
        frame.append(static_cast<char>(value & 0xFF));
    };

    if (function >= 0x01 && function <= 0x04) {
        appendWord(static_cast<quint16>(quantity));
    } else if (function == 0x05) {
        const QString value = m_writeDataEdit->text().trimmed().toLower();
        if (value != QStringLiteral("0") && value != QStringLiteral("1")
            && value != QStringLiteral("off") && value != QStringLiteral("on")) {
            *errorMessage = QStringLiteral("写单个线圈的数据只能是 0/1 或 off/on");
            return {};
        }
        appendWord((value == QStringLiteral("1") || value == QStringLiteral("on"))
                       ? 0xFF00 : 0x0000);
    } else if (function == 0x06) {
        bool ok = false;
        QString valueText = m_writeDataEdit->text().trimmed();
        const uint value = valueText.toUInt(&ok, valueText.startsWith(QStringLiteral("0x"),
                                                                      Qt::CaseInsensitive)
                                                     ? 16 : 10);
        if (!ok || value > 0xFFFF) {
            *errorMessage = QStringLiteral("请输入 0 到 65535 之间的寄存器值");
            return {};
        }
        appendWord(static_cast<quint16>(value));
    } else if (function == 0x0F || function == 0x10) {
        bool ok = false;
        const QList<quint16> values = parseValues(m_writeDataEdit->text(), &ok);
        if (!ok || values.isEmpty()) {
            *errorMessage = QStringLiteral("写入数据格式不正确，请使用空格或逗号分隔");
            return {};
        }
        if (values.size() != quantity) {
            *errorMessage = QStringLiteral("写入数据有 %1 项，与数量 %2 不一致")
                                .arg(values.size()).arg(quantity);
            return {};
        }
        appendWord(static_cast<quint16>(quantity));
        if (function == 0x0F) {
            for (quint16 value : values) {
                if (value > 1) {
                    *errorMessage = QStringLiteral("写多个线圈时，每项数据只能是 0 或 1");
                    return {};
                }
            }
            const int byteCount = (quantity + 7) / 8;
            frame.append(static_cast<char>(byteCount));
            for (int byteIndex = 0; byteIndex < byteCount; ++byteIndex) {
                quint8 packed = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    const int valueIndex = byteIndex * 8 + bit;
                    if (valueIndex < values.size() && values.at(valueIndex))
                        packed |= static_cast<quint8>(1u << bit);
                }
                frame.append(static_cast<char>(packed));
            }
        } else {
            frame.append(static_cast<char>(quantity * 2));
            for (quint16 value : values)
                appendWord(value);
        }
    }

    return appendCrc(frame);
}

void MainWindow::sendRequest()
{
    if (isSlaveMode()) {
        setStatus(QStringLiteral("从站模式由主站发起请求，本机仅监听并响应"), true);
        return;
    }
    if (!isTransportOpen()) {
        setStatus(isTcpMode() ? QStringLiteral("请先建立 TCP 连接")
                              : QStringLiteral("请先打开串口"), true);
        return;
    }
    if (m_waitingForResponse)
        return;

    QString error;
    const QByteArray frame = buildRequest(&error);
    if (frame.isEmpty()) {
        setStatus(error, true);
        m_writeDataEdit->setFocus();
        return;
    }

    const QString description = m_functionCombo->currentText();
    m_rawRequest = false;
    writeFrame(frame, description);
}

void MainWindow::sendRawFrame()
{
    if (isSlaveMode()) {
        setStatus(QStringLiteral("从站模式下不能主动发送原始请求"), true);
        return;
    }
    if (!isTransportOpen()) {
        setStatus(isTcpMode() ? QStringLiteral("请先建立 TCP 连接")
                              : QStringLiteral("请先打开串口"), true);
        return;
    }
    if (m_waitingForResponse)
        return;

    bool ok = false;
    QByteArray frame = parseHex(m_rawEdit->text(), &ok);
    if (!ok || frame.isEmpty()) {
        setStatus(QStringLiteral("十六进制报文格式不正确"), true);
        m_rawEdit->setFocus();
        return;
    }
    if (m_autoCrcCheck->isChecked())
        frame = appendCrc(frame);
    else if (frame.size() >= 4 && !hasValidCrc(frame)) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("CRC 校验"),
            QStringLiteral("当前报文的 CRC 校验不通过，仍要发送吗？"));
        if (answer != QMessageBox::Yes)
            return;
    }

    m_rawRequest = true;
    writeFrame(frame, QStringLiteral("原始报文"));
}

void MainWindow::writeFrame(const QByteArray &frame, const QString &description)
{
    m_receiveBuffer.clear();
    m_tcpReceiveBuffer.clear();
    QByteArray wireFrame = frame;
    if (isTcpMode()) {
        m_pendingTransactionId = ++m_transactionId;
        wireFrame = encodeTcp(frame.left(frame.size() - 2), m_pendingTransactionId);
    } else if (isAsciiMode()) {
        wireFrame = encodeAscii(frame.left(frame.size() - 2));
    }
    const qint64 written = isTcpMode() ? m_tcpSocket->write(wireFrame)
                                      : m_serial->write(wireFrame);
    if (written < 0) {
        const QString error = isTcpMode() ? m_tcpSocket->errorString()
                                          : m_serial->errorString();
        setStatus(QStringLiteral("发送失败：%1").arg(error), true);
        return;
    }

    m_lastRequest = frame;
    m_waitingForResponse = true;
    m_responseTimer->start(m_timeoutSpin->value());
    setBusyState(true);
    ++m_txCount;
    m_counterLabel->setText(QStringLiteral("TX %1  ·  RX %2").arg(m_txCount).arg(m_rxCount));
    appendLog(QStringLiteral("TX"), wireFrame, description);
    setStatus(QStringLiteral("请求已发送，等待响应…"));
}

void MainWindow::readSerialData()
{
    m_receiveBuffer.append(m_serial->readAll());
    if (isAsciiMode()) {
        int frameEnd = -1;
        while ((frameEnd = m_receiveBuffer.indexOf("\r\n")) >= 0) {
            const QByteArray wireFrame = m_receiveBuffer.left(frameEnd + 2);
            m_receiveBuffer.remove(0, frameEnd + 2);
            processWireFrame(wireFrame);
        }
    } else {
        m_frameGapTimer->start();
    }
}

void MainWindow::acceptTcpClient()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *client = m_tcpServer->nextPendingConnection();
        if (m_slaveTcpClient && m_slaveTcpClient != client) {
            m_slaveTcpClient->disconnectFromHost();
            m_slaveTcpClient->deleteLater();
        }
        m_slaveTcpClient = client;
        m_tcpReceiveBuffer.clear();
        connect(client, &QTcpSocket::readyRead, this, &MainWindow::readTcpData);
        connect(client, &QTcpSocket::disconnected, this, [this, client] {
            appendLog(QStringLiteral("SYS"), {},
                      QStringLiteral("TCP 主站已断开：%1").arg(client->peerAddress().toString()));
            if (m_slaveTcpClient == client)
                m_slaveTcpClient = nullptr;
            client->deleteLater();
        });
        appendLog(QStringLiteral("SYS"), {},
                  QStringLiteral("TCP 主站已连接：%1:%2")
                      .arg(client->peerAddress().toString()).arg(client->peerPort()));
        setStatus(QStringLiteral("TCP 从站正在服务 %1").arg(client->peerAddress().toString()));
    }
}

void MainWindow::readTcpData()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        socket = m_tcpSocket;
    m_tcpReceiveBuffer.append(socket->readAll());

    while (m_tcpReceiveBuffer.size() >= 6) {
        const quint16 length = readU16(m_tcpReceiveBuffer, 4);
        if (length < 2 || length > 260) {
            appendLog(QStringLiteral("ERR"), m_tcpReceiveBuffer,
                      QStringLiteral("无效的 Modbus TCP 长度字段"), true);
            m_tcpReceiveBuffer.clear();
            return;
        }
        const int totalLength = 6 + length;
        if (m_tcpReceiveBuffer.size() < totalLength)
            return;
        const QByteArray wireFrame = m_tcpReceiveBuffer.left(totalLength);
        m_tcpReceiveBuffer.remove(0, totalLength);
        processWireFrame(wireFrame);
    }
}

void MainWindow::processReceivedFrame()
{
    if (m_receiveBuffer.isEmpty())
        return;

    const QByteArray wireFrame = m_receiveBuffer;
    m_receiveBuffer.clear();
    processWireFrame(wireFrame);
}

void MainWindow::processWireFrame(const QByteArray &wireFrame)
{
    QByteArray frame = wireFrame;
    if (isTcpMode()) {
        if (wireFrame.size() < 8 || readU16(wireFrame, 2) != 0
            || wireFrame.size() != 6 + readU16(wireFrame, 4)) {
            appendLog(QStringLiteral("ERR"), wireFrame,
                      QStringLiteral("Modbus TCP MBAP 头无效"), true);
            setStatus(QStringLiteral("收到无效的 Modbus TCP 报文"), true);
            return;
        }
        const quint16 transaction = readU16(wireFrame, 0);
        if (!isSlaveMode() && transaction != m_pendingTransactionId) {
            appendLog(QStringLiteral("ERR"), wireFrame,
                      QStringLiteral("TCP 事务标识不匹配"), true);
            setStatus(QStringLiteral("TCP 事务标识不匹配"), true);
            return;
        }
        m_currentTransactionId = transaction;
        frame = appendCrc(wireFrame.mid(6, readU16(wireFrame, 4)));
    } else if (isAsciiMode()) {
        bool ok = false;
        const QByteArray payload = decodeAscii(wireFrame, &ok);
        if (!ok) {
            appendLog(QStringLiteral("ERR"), wireFrame,
                      QStringLiteral("Modbus ASCII 格式或 LRC 校验失败"), true);
            setStatus(QStringLiteral("ASCII 报文格式或 LRC 校验失败"), true);
            return;
        }
        frame = appendCrc(payload);
    }

    m_responseTimer->stop();
    m_waitingForResponse = false;
    setBusyState(false);
    ++m_rxCount;
    m_counterLabel->setText(QStringLiteral("TX %1  ·  RX %2").arg(m_txCount).arg(m_rxCount));
    appendLog(QStringLiteral("RX"), wireFrame,
              isAsciiMode() ? QString::fromLatin1(wireFrame).trimmed() : QString());

    if (frame.size() < 5) {
        setStatus(QStringLiteral("%1过短（%2 字节）")
                      .arg(isSlaveMode() ? QStringLiteral("请求") : QStringLiteral("响应"))
                      .arg(frame.size()), true);
        return;
    }
    if (!hasValidCrc(frame)) {
        setStatus(QStringLiteral("%1 CRC 校验失败")
                      .arg(isSlaveMode() ? QStringLiteral("请求") : QStringLiteral("响应"))
                      + QStringLiteral("，请检查串口参数和线路"), true);
        appendLog(QStringLiteral("ERR"), {}, QStringLiteral("CRC 校验失败"), true);
        return;
    }

    if (isSlaveMode()) {
        handleSlaveRequest(frame);
        return;
    }
    if (m_rawRequest) {
        setStatus(QStringLiteral("收到有效响应，共 %1 字节").arg(frame.size()));
        return;
    }
    parseResponse(frame);
}

void MainWindow::parseResponse(const QByteArray &frame)
{
    const quint8 slave = static_cast<quint8>(frame.at(0));
    const quint8 function = static_cast<quint8>(frame.at(1));
    const quint8 expectedSlave = static_cast<quint8>(m_lastRequest.at(0));
    const quint8 expectedFunction = static_cast<quint8>(m_lastRequest.at(1));

    if (slave != expectedSlave) {
        setStatus(QStringLiteral("响应从站地址不匹配：期望 %1，收到 %2")
                      .arg(expectedSlave).arg(slave), true);
        return;
    }
    if (function & 0x80) {
        const quint8 code = static_cast<quint8>(frame.at(2));
        const QString message = QStringLiteral("Modbus 异常 %1：%2")
                                    .arg(code).arg(exceptionText(code));
        setStatus(message, true);
        appendLog(QStringLiteral("ERR"), {}, message, true);
        return;
    }
    if (function != expectedFunction) {
        setStatus(QStringLiteral("响应功能码不匹配：期望 0x%1，收到 0x%2")
                      .arg(expectedFunction, 2, 16, QLatin1Char('0'))
                      .arg(function, 2, 16, QLatin1Char('0')).toUpper(), true);
        return;
    }

    m_resultTable->setRowCount(0);
    const quint16 baseAddress = readU16(m_lastRequest, 2);

    if (function == 0x01 || function == 0x02) {
        const int byteCount = static_cast<quint8>(frame.at(2));
        if (frame.size() != byteCount + 5) {
            setStatus(QStringLiteral("响应长度与字节计数不一致"), true);
            return;
        }
        const int requested = readU16(m_lastRequest, 4);
        for (int index = 0; index < requested; ++index) {
            const int byteIndex = 3 + index / 8;
            if (byteIndex >= 3 + byteCount)
                break;
            const bool on = (static_cast<quint8>(frame.at(byteIndex)) >> (index % 8)) & 0x01;
            const int row = m_resultTable->rowCount();
            m_resultTable->insertRow(row);
            m_resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(baseAddress + index)));
            m_resultTable->setItem(row, 1, new QTableWidgetItem(on ? QStringLiteral("1") : QStringLiteral("0")));
            m_resultTable->setItem(row, 2, new QTableWidgetItem(on ? QStringLiteral("0x01") : QStringLiteral("0x00")));
            m_resultTable->setItem(row, 3, new QTableWidgetItem(on ? QStringLiteral("ON / 接通") : QStringLiteral("OFF / 断开")));
        }
        setStatus(QStringLiteral("读取成功：%1 个位状态").arg(m_resultTable->rowCount()));
    } else if (function == 0x03 || function == 0x04) {
        const int byteCount = static_cast<quint8>(frame.at(2));
        if ((byteCount % 2) != 0 || frame.size() != byteCount + 5) {
            setStatus(QStringLiteral("寄存器响应长度不正确"), true);
            return;
        }
        const int registerCount = byteCount / 2;
        for (int index = 0; index < registerCount; ++index) {
            const quint16 value = readU16(frame, 3 + index * 2);
            const int row = m_resultTable->rowCount();
            m_resultTable->insertRow(row);
            m_resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(baseAddress + index)));
            m_resultTable->setItem(row, 1, new QTableWidgetItem(QString::number(value)));
            m_resultTable->setItem(row, 2, new QTableWidgetItem(
                QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper()));
            m_resultTable->setItem(row, 3, new QTableWidgetItem(
                QStringLiteral("%1").arg(value, 16, 2, QLatin1Char('0'))));
        }
        setStatus(QStringLiteral("读取成功：%1 个寄存器").arg(registerCount));
    } else if (function == 0x05 || function == 0x06) {
        if (frame.size() != 8 || frame.left(6) != m_lastRequest.left(6)) {
            setStatus(QStringLiteral("写入响应与请求内容不一致"), true);
            return;
        }
        const quint16 value = readU16(frame, 4);
        m_resultTable->insertRow(0);
        m_resultTable->setItem(0, 0, new QTableWidgetItem(QString::number(baseAddress)));
        m_resultTable->setItem(0, 1, new QTableWidgetItem(QString::number(value)));
        m_resultTable->setItem(0, 2, new QTableWidgetItem(
            QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper()));
        m_resultTable->setItem(0, 3, new QTableWidgetItem(QStringLiteral("写入成功")));
        setStatus(QStringLiteral("单个数据写入成功"));
    } else if (function == 0x0F || function == 0x10) {
        if (frame.size() != 8) {
            setStatus(QStringLiteral("写多个数据的响应长度不正确"), true);
            return;
        }
        const quint16 responseAddress = readU16(frame, 2);
        const quint16 responseQuantity = readU16(frame, 4);
        if (responseAddress != baseAddress || responseQuantity != readU16(m_lastRequest, 4)) {
            setStatus(QStringLiteral("写入响应的地址或数量不匹配"), true);
            return;
        }
        m_resultTable->insertRow(0);
        m_resultTable->setItem(0, 0, new QTableWidgetItem(QString::number(responseAddress)));
        m_resultTable->setItem(0, 1, new QTableWidgetItem(QString::number(responseQuantity)));
        m_resultTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("—")));
        m_resultTable->setItem(0, 3, new QTableWidgetItem(QStringLiteral("批量写入成功")));
        setStatus(QStringLiteral("批量写入成功：%1 项").arg(responseQuantity));
    }
}

void MainWindow::handleSlaveRequest(const QByteArray &frame)
{
    const quint8 requestUnit = static_cast<quint8>(frame.at(0));
    const quint8 function = static_cast<quint8>(frame.at(1));
    const quint8 configuredUnit = static_cast<quint8>(m_slaveUnitSpin->value());
    const bool broadcast = requestUnit == 0;

    if (!broadcast && requestUnit != configuredUnit) {
        setStatus(QStringLiteral("忽略发往从站 %1 的请求").arg(requestUnit));
        return;
    }

    auto sendException = [&](quint8 code) {
        if (broadcast)
            return;
        QByteArray response;
        response.append(static_cast<char>(configuredUnit));
        response.append(static_cast<char>(function | 0x80));
        response.append(static_cast<char>(code));
        sendSlaveResponse(appendCrc(response),
                          QStringLiteral("异常响应：%1").arg(exceptionText(code)));
    };
    auto validRange = [](quint16 address, quint16 quantity) {
        return static_cast<quint32>(address) + quantity <= 65536u;
    };

    if (broadcast && function >= 0x01 && function <= 0x04) {
        setStatus(QStringLiteral("已忽略广播读请求"));
        return;
    }

    if (function >= 0x01 && function <= 0x04) {
        if (frame.size() != 8) {
            sendException(0x03);
            return;
        }
        const quint16 address = readU16(frame, 2);
        const quint16 quantity = readU16(frame, 4);
        const quint16 protocolMaximum = (function <= 0x02) ? 2000 : 125;
        if (quantity == 0 || quantity > protocolMaximum) {
            sendException(0x03);
            return;
        }
        if (!validRange(address, quantity)) {
            sendException(0x02);
            return;
        }

        QByteArray response;
        response.append(static_cast<char>(configuredUnit));
        response.append(static_cast<char>(function));
        if (function == 0x01 || function == 0x02) {
            const int byteCount = (quantity + 7) / 8;
            response.append(static_cast<char>(byteCount));
            const QVector<quint8> &source =
                function == 0x01 ? m_coils : m_discreteInputs;
            for (int byteIndex = 0; byteIndex < byteCount; ++byteIndex) {
                quint8 packed = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    const int offset = byteIndex * 8 + bit;
                    if (offset < quantity && source.at(address + offset))
                        packed |= static_cast<quint8>(1u << bit);
                }
                response.append(static_cast<char>(packed));
            }
        } else {
            response.append(static_cast<char>(quantity * 2));
            const QVector<quint16> &source =
                function == 0x03 ? m_holdingRegisters : m_inputRegisters;
            for (int offset = 0; offset < quantity; ++offset) {
                const quint16 value = source.at(address + offset);
                response.append(static_cast<char>((value >> 8) & 0xFF));
                response.append(static_cast<char>(value & 0xFF));
            }
        }
        sendSlaveResponse(appendCrc(response),
                          QStringLiteral("从站读取响应，地址 %1，数量 %2")
                              .arg(address).arg(quantity));
        return;
    }

    if (function == 0x05 || function == 0x06) {
        if (frame.size() != 8) {
            sendException(0x03);
            return;
        }
        const quint16 address = readU16(frame, 2);
        const quint16 value = readU16(frame, 4);
        if (!validRange(address, 1)) {
            sendException(0x02);
            return;
        }
        if (function == 0x05) {
            if (value != 0x0000 && value != 0xFF00) {
                sendException(0x03);
                return;
            }
            m_coils[address] = value == 0xFF00 ? 1 : 0;
        } else {
            m_holdingRegisters[address] = value;
        }
        refreshSlaveDataTable();
        if (!broadcast) {
            QByteArray response = frame.left(6);
            response[0] = static_cast<char>(configuredUnit);
            sendSlaveResponse(appendCrc(response),
                              QStringLiteral("单个数据写入，地址 %1").arg(address));
        } else {
            setStatus(QStringLiteral("已执行广播写入，地址 %1").arg(address));
        }
        return;
    }

    if (function == 0x0F || function == 0x10) {
        if (frame.size() < 10) {
            sendException(0x03);
            return;
        }
        const quint16 address = readU16(frame, 2);
        const quint16 quantity = readU16(frame, 4);
        const int byteCount = static_cast<quint8>(frame.at(6));
        const int expectedByteCount = function == 0x0F ? (quantity + 7) / 8
                                                       : quantity * 2;
        const quint16 protocolMaximum = function == 0x0F ? 1968 : 123;
        if (quantity == 0 || quantity > protocolMaximum || byteCount != expectedByteCount
            || frame.size() != 9 + byteCount) {
            sendException(0x03);
            return;
        }
        if (!validRange(address, quantity)) {
            sendException(0x02);
            return;
        }

        if (function == 0x0F) {
            for (int offset = 0; offset < quantity; ++offset) {
                const quint8 packed = static_cast<quint8>(frame.at(7 + offset / 8));
                m_coils[address + offset] = (packed >> (offset % 8)) & 0x01;
            }
        } else {
            for (int offset = 0; offset < quantity; ++offset)
                m_holdingRegisters[address + offset] = readU16(frame, 7 + offset * 2);
        }
        refreshSlaveDataTable();
        if (!broadcast) {
            QByteArray response;
            response.append(static_cast<char>(configuredUnit));
            response.append(static_cast<char>(function));
            response.append(frame.mid(2, 4));
            sendSlaveResponse(appendCrc(response),
                              QStringLiteral("批量写入，地址 %1，数量 %2")
                                  .arg(address).arg(quantity));
        } else {
            setStatus(QStringLiteral("已执行广播批量写入，地址 %1，数量 %2")
                          .arg(address).arg(quantity));
        }
        return;
    }

    sendException(0x01);
}

void MainWindow::sendSlaveResponse(const QByteArray &frame, const QString &description)
{
    if (!isTransportOpen())
        return;

    QByteArray wireFrame = frame;
    qint64 written = -1;
    QString error;
    if (isTcpMode()) {
        if (!m_slaveTcpClient
            || m_slaveTcpClient->state() != QAbstractSocket::ConnectedState)
            return;
        wireFrame = encodeTcp(frame.left(frame.size() - 2), m_currentTransactionId);
        written = m_slaveTcpClient->write(wireFrame);
        error = m_slaveTcpClient->errorString();
    } else {
        if (isAsciiMode())
            wireFrame = encodeAscii(frame.left(frame.size() - 2));
        written = m_serial->write(wireFrame);
        error = m_serial->errorString();
    }
    if (written != wireFrame.size()) {
        setStatus(QStringLiteral("从站响应发送失败：%1").arg(error), true);
        return;
    }
    ++m_txCount;
    m_counterLabel->setText(QStringLiteral("TX %1  ·  RX %2").arg(m_txCount).arg(m_rxCount));
    appendLog(QStringLiteral("TX"), wireFrame, description);
    setStatus(description);
}

void MainWindow::handleResponseTimeout()
{
    m_waitingForResponse = false;
    m_receiveBuffer.clear();
    m_tcpReceiveBuffer.clear();
    setBusyState(false);
    const QString message = QStringLiteral(
        "响应超时（%1 ms），请检查从站地址、线路和通讯参数").arg(m_timeoutSpin->value());
    setStatus(message, true);
    appendLog(QStringLiteral("ERR"), {}, message, true);
}

void MainWindow::handleSerialError()
{
    const auto error = m_serial->error();
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError
        || error == QSerialPort::NotOpenError)
        return;

    const QString message = QStringLiteral("串口错误：%1").arg(m_serial->errorString());
    setStatus(message, true);
    appendLog(QStringLiteral("ERR"), {}, message, true);
    if (error == QSerialPort::ResourceError && m_serial->isOpen()) {
        m_pollCheck->setChecked(false);
        m_serial->close();
        m_waitingForResponse = false;
        setConnectedState(false);
    }
}

void MainWindow::togglePolling(bool enabled)
{
    if (enabled) {
        if (isSlaveMode()) {
            m_pollCheck->setChecked(false);
            return;
        }
        if (!isTransportOpen()) {
            m_pollCheck->setChecked(false);
            setStatus(isTcpMode() ? QStringLiteral("请先建立 TCP 连接")
                                  : QStringLiteral("请先打开串口"), true);
            return;
        }
        m_pollTimer->start(m_pollIntervalSpin->value());
        if (!m_waitingForResponse)
            sendRequest();
        setStatus(QStringLiteral("定时轮询已开启，间隔 %1 ms").arg(m_pollIntervalSpin->value()));
    } else {
        m_pollTimer->stop();
    }
}

void MainWindow::appendLog(const QString &direction, const QByteArray &data,
                           const QString &message, bool isError)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    QString line = QStringLiteral("[%1]  %2").arg(timestamp, direction.leftJustified(3));
    if (!data.isEmpty())
        line += QStringLiteral("  ") + toHex(data);
    if (!message.isEmpty())
        line += QStringLiteral("  |  ") + message;
    line += QLatin1Char('\n');

    QColor color(QStringLiteral("#CBD5E1"));
    if (direction == QStringLiteral("TX"))
        color = QColor(QStringLiteral("#60A5FA"));
    else if (direction == QStringLiteral("RX"))
        color = QColor(QStringLiteral("#34D399"));
    else if (isError || direction == QStringLiteral("ERR"))
        color = QColor(QStringLiteral("#FB7185"));

    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(color);
    cursor.insertText(line, format);
    m_logEdit->setTextCursor(cursor);
    m_logEdit->ensureCursorVisible();
}

void MainWindow::clearLog()
{
    m_logEdit->clear();
    m_txCount = 0;
    m_rxCount = 0;
    m_counterLabel->setText(QStringLiteral("TX 0  ·  RX 0"));
}

void MainWindow::saveLog()
{
    const QString suggested = QStringLiteral("modbus_log_%1.txt")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存通讯日志"), suggested, QStringLiteral("文本文件 (*.txt)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), file.errorString());
        return;
    }
    file.write(m_logEdit->toPlainText().toUtf8());
    setStatus(QStringLiteral("日志已保存：%1").arg(path));
}

void MainWindow::setStatus(const QString &text, bool error)
{
    m_statusLabel->setText((error ? QStringLiteral("错误：") : QStringLiteral("状态：")) + text);
    m_statusLabel->setProperty("error", error);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

quint16 MainWindow::modbusCrc(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x0001) ? static_cast<quint16>((crc >> 1) ^ 0xA001)
                                 : static_cast<quint16>(crc >> 1);
    }
    return crc;
}

QByteArray MainWindow::appendCrc(QByteArray data)
{
    const quint16 crc = modbusCrc(data);
    data.append(static_cast<char>(crc & 0xFF));
    data.append(static_cast<char>((crc >> 8) & 0xFF));
    return data;
}

bool MainWindow::hasValidCrc(const QByteArray &data)
{
    if (data.size() < 4)
        return false;
    const QByteArray payload = data.left(data.size() - 2);
    const quint16 expected = modbusCrc(payload);
    const quint16 actual = static_cast<quint8>(data.at(data.size() - 2))
                           | (static_cast<quint8>(data.at(data.size() - 1)) << 8);
    return expected == actual;
}

quint8 MainWindow::modbusLrc(const QByteArray &data)
{
    quint8 sum = 0;
    for (char byte : data)
        sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
    return static_cast<quint8>(-sum);
}

QByteArray MainWindow::encodeAscii(const QByteArray &payload)
{
    QByteArray body = payload;
    body.append(static_cast<char>(modbusLrc(payload)));
    return QByteArrayLiteral(":") + body.toHex().toUpper() + QByteArrayLiteral("\r\n");
}

QByteArray MainWindow::decodeAscii(const QByteArray &frame, bool *ok)
{
    QByteArray ascii = frame.trimmed();
    if (!ascii.startsWith(':')) {
        *ok = false;
        return {};
    }
    ascii.remove(0, 1);
    if (ascii.size() < 6 || (ascii.size() % 2) != 0) {
        *ok = false;
        return {};
    }
    for (char character : ascii) {
        const bool hexDigit = (character >= '0' && character <= '9')
                              || (character >= 'a' && character <= 'f')
                              || (character >= 'A' && character <= 'F');
        if (!hexDigit) {
            *ok = false;
            return {};
        }
    }
    QByteArray bytes = QByteArray::fromHex(ascii);
    quint8 sum = 0;
    for (char byte : bytes)
        sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
    if (sum != 0) {
        *ok = false;
        return {};
    }
    bytes.chop(1);
    *ok = true;
    return bytes;
}

QByteArray MainWindow::encodeTcp(const QByteArray &payload, quint16 transactionId) const
{
    QByteArray frame;
    frame.reserve(payload.size() + 6);
    frame.append(static_cast<char>((transactionId >> 8) & 0xFF));
    frame.append(static_cast<char>(transactionId & 0xFF));
    frame.append('\0');
    frame.append('\0');
    const quint16 length = static_cast<quint16>(payload.size());
    frame.append(static_cast<char>((length >> 8) & 0xFF));
    frame.append(static_cast<char>(length & 0xFF));
    frame.append(payload);
    return frame;
}

QString MainWindow::toHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QByteArray MainWindow::parseHex(const QString &text, bool *ok)
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("[\\s,:;-]")));
    compact.remove(QRegularExpression(QStringLiteral("0[xX]")));
    if (compact.isEmpty() || compact.size() % 2 != 0
        || compact.contains(QRegularExpression(QStringLiteral("[^0-9a-fA-F]")))) {
        *ok = false;
        return {};
    }
    *ok = true;
    return QByteArray::fromHex(compact.toLatin1());
}

QList<quint16> MainWindow::parseValues(const QString &text, bool *ok)
{
    QList<quint16> result;
    const QStringList parts = text.split(
        QRegularExpression(QStringLiteral("[\\s,;，；]+")), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        *ok = false;
        return result;
    }
    for (const QString &part : parts) {
        bool valueOk = false;
        const int base = part.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ? 16 : 10;
        const uint value = part.toUInt(&valueOk, base);
        if (!valueOk || value > 0xFFFF) {
            *ok = false;
            return {};
        }
        result.append(static_cast<quint16>(value));
    }
    *ok = true;
    return result;
}

QString MainWindow::exceptionText(quint8 code)
{
    switch (code) {
    case 0x01: return QStringLiteral("非法功能");
    case 0x02: return QStringLiteral("非法数据地址");
    case 0x03: return QStringLiteral("非法数据值");
    case 0x04: return QStringLiteral("从站设备故障");
    case 0x05: return QStringLiteral("确认，正在处理");
    case 0x06: return QStringLiteral("从站设备忙");
    case 0x08: return QStringLiteral("存储奇偶校验错误");
    case 0x0A: return QStringLiteral("网关路径不可用");
    case 0x0B: return QStringLiteral("网关目标设备无响应");
    default: return QStringLiteral("未知异常");
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());

    auto setComboByData = [](QComboBox *combo, const QVariant &value) {
        const int index = combo->findData(value);
        if (index >= 0)
            combo->setCurrentIndex(index);
    };
    setComboByData(m_protocolCombo, settings.value(QStringLiteral("transport/protocol"), 0));
    setComboByData(m_baudCombo, settings.value(QStringLiteral("serial/baud"), 9600));
    setComboByData(m_modeCombo, settings.value(QStringLiteral("serial/mode"), 0));
    setComboByData(m_dataBitsCombo, settings.value(QStringLiteral("serial/dataBits"),
                                                   QSerialPort::Data8));
    setComboByData(m_parityCombo, settings.value(QStringLiteral("serial/parity"),
                                                 QSerialPort::NoParity));
    setComboByData(m_stopBitsCombo, settings.value(QStringLiteral("serial/stopBits"),
                                                   QSerialPort::OneStop));
    setComboByData(m_flowControlCombo, settings.value(QStringLiteral("serial/flow"),
                                                       QSerialPort::NoFlowControl));
    m_hostEdit->setText(settings.value(QStringLiteral("tcp/host"),
                                       QStringLiteral("127.0.0.1")).toString());
    m_tcpPortSpin->setValue(settings.value(QStringLiteral("tcp/port"), 502).toInt());
    m_slaveSpin->setValue(settings.value(QStringLiteral("request/slave"), 1).toInt());
    m_slaveUnitSpin->setValue(settings.value(QStringLiteral("slave/unit"), 1).toInt());
    m_slaveViewStartSpin->setValue(settings.value(QStringLiteral("slave/viewStart"), 0).toInt());
    m_slaveViewCountSpin->setValue(settings.value(QStringLiteral("slave/viewCount"), 20).toInt());
    setComboByData(m_slaveAreaCombo, settings.value(QStringLiteral("slave/area"), 2));
    m_addressSpin->setValue(settings.value(QStringLiteral("request/address"), 0).toInt());
    m_quantitySpin->setValue(settings.value(QStringLiteral("request/quantity"), 10).toInt());
    setComboByData(m_functionCombo, settings.value(QStringLiteral("request/function"), 3));
    m_timeoutSpin->setValue(settings.value(QStringLiteral("timing/timeout"), 1000).toInt());
    m_pollIntervalSpin->setValue(settings.value(QStringLiteral("timing/pollInterval"), 1000).toInt());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("transport/protocol"), m_protocolCombo->currentData());
    settings.setValue(QStringLiteral("serial/baud"), m_baudCombo->currentData());
    settings.setValue(QStringLiteral("serial/mode"), m_modeCombo->currentData());
    settings.setValue(QStringLiteral("serial/dataBits"), m_dataBitsCombo->currentData());
    settings.setValue(QStringLiteral("serial/parity"), m_parityCombo->currentData());
    settings.setValue(QStringLiteral("serial/stopBits"), m_stopBitsCombo->currentData());
    settings.setValue(QStringLiteral("serial/flow"), m_flowControlCombo->currentData());
    settings.setValue(QStringLiteral("tcp/host"), m_hostEdit->text().trimmed());
    settings.setValue(QStringLiteral("tcp/port"), m_tcpPortSpin->value());
    settings.setValue(QStringLiteral("request/slave"), m_slaveSpin->value());
    settings.setValue(QStringLiteral("slave/unit"), m_slaveUnitSpin->value());
    settings.setValue(QStringLiteral("slave/viewStart"), m_slaveViewStartSpin->value());
    settings.setValue(QStringLiteral("slave/viewCount"), m_slaveViewCountSpin->value());
    settings.setValue(QStringLiteral("slave/area"), m_slaveAreaCombo->currentData());
    settings.setValue(QStringLiteral("request/function"), m_functionCombo->currentData());
    settings.setValue(QStringLiteral("request/address"), m_addressSpin->value());
    settings.setValue(QStringLiteral("request/quantity"), m_quantitySpin->value());
    settings.setValue(QStringLiteral("timing/timeout"), m_timeoutSpin->value());
    settings.setValue(QStringLiteral("timing/pollInterval"), m_pollIntervalSpin->value());
}
