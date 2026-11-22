#include "mainwindow.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSerialPortInfo>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVector>
#include <QVBoxLayout>
#include <QtMath>

#include <limits>

namespace
{
QFrame *makeMetricCard(const QString &title, QLabel **valueLabel)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("metricCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(9, 6, 9, 6);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("metricTitle"));
    auto *value = new QLabel(QStringLiteral("--"));
    value->setObjectName(QStringLiteral("metricValue"));
    value->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    *valueLabel = value;
    return card;
}

QString printableOrDash(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("--") : value;
}

void selectComboData(QComboBox *combo, int value)
{
    const int index = combo->findData(value);
    if (index >= 0)
        combo->setCurrentIndex(index);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_serial(new QSerialPort(this))
{
    setWindowTitle(QStringLiteral("XDriverTool"));
    resize(1360, 1040);
    setMinimumSize(1024, 820);

    buildUi();
    applyStyle();

    connect(m_serial, &QSerialPort::readyRead, this, [this] { readSerialData(); });
    connect(m_serial, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) { handleSerialError(error); });
    connect(&m_pollTimer, &QTimer::timeout, this, [this] { pollStatus(); });
    m_pollTimer.setInterval(200);
    connect(&m_statusTimeout, &QTimer::timeout, this, [this] {
        m_statusRequestPending = false;
        m_lastUpdateLabel->setText(QStringLiteral("状态刷新超时"));
        m_lastUpdateLabel->setStyleSheet(QStringLiteral("color: #f0ad4e;"));
    });
    m_statusTimeout.setSingleShot(true);
    connect(&m_commandTimer, &QTimer::timeout, this, [this] { dispatchNextCommand(); });
    m_commandTimer.setSingleShot(true);

    loadSerialSettings();
    refreshPorts();
    updateConnectionUi();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);

    auto *sidebarContent = new QWidget;
    auto *sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(4, 4, 4, 4);
    sidebarLayout->setSpacing(6);

    auto *connectionGroup = new QGroupBox(QStringLiteral("串口配置"));
    auto *connectionLayout = new QGridLayout(connectionGroup);
    connectionLayout->setContentsMargins(3, 8, 3, 7);
    connectionLayout->setHorizontalSpacing(4);
    connectionLayout->setVerticalSpacing(4);
    m_portCombo = new QComboBox;
    m_portCombo->setEditable(true);
    m_portCombo->setInsertPolicy(QComboBox::NoInsert);
    m_portCombo->setMinimumWidth(0);
    m_portCombo->setFixedWidth(100);
    m_portCombo->setPlaceholderText(QStringLiteral("例如 COM3"));
    m_refreshPortsButton = new QPushButton(QStringLiteral("刷新"));
    m_refreshPortsButton->setToolTip(QStringLiteral("重新扫描可用串口；也可以手动输入端口号"));
    m_baudCombo = new QComboBox;
    for (const int baudRate : {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
        m_baudCombo->addItem(QString::number(baudRate));
    m_baudCombo->setEditable(true);
    m_baudCombo->setFixedWidth(100);
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem(QStringLiteral("5"), static_cast<int>(QSerialPort::Data5));
    m_dataBitsCombo->addItem(QStringLiteral("6"), static_cast<int>(QSerialPort::Data6));
    m_dataBitsCombo->addItem(QStringLiteral("7"), static_cast<int>(QSerialPort::Data7));
    m_dataBitsCombo->addItem(QStringLiteral("8"), static_cast<int>(QSerialPort::Data8));
    m_dataBitsCombo->setFixedWidth(100);
    m_parityCombo = new QComboBox;
    m_parityCombo->addItem(QStringLiteral("无"), static_cast<int>(QSerialPort::NoParity));
    m_parityCombo->addItem(QStringLiteral("偶"), static_cast<int>(QSerialPort::EvenParity));
    m_parityCombo->addItem(QStringLiteral("奇"), static_cast<int>(QSerialPort::OddParity));
    m_parityCombo->addItem(QStringLiteral("标记"), static_cast<int>(QSerialPort::MarkParity));
    m_parityCombo->addItem(QStringLiteral("空格"), static_cast<int>(QSerialPort::SpaceParity));
    m_parityCombo->setFixedWidth(100);
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem(QStringLiteral("1"), static_cast<int>(QSerialPort::OneStop));
    m_stopBitsCombo->addItem(QStringLiteral("1.5"), static_cast<int>(QSerialPort::OneAndHalfStop));
    m_stopBitsCombo->addItem(QStringLiteral("2"), static_cast<int>(QSerialPort::TwoStop));
    m_stopBitsCombo->setFixedWidth(100);
    m_flowControlCombo = new QComboBox;
    m_flowControlCombo->addItem(QStringLiteral("无"), static_cast<int>(QSerialPort::NoFlowControl));
    m_flowControlCombo->addItem(QStringLiteral("软件"), static_cast<int>(QSerialPort::SoftwareControl));
    m_flowControlCombo->addItem(QStringLiteral("硬件"), static_cast<int>(QSerialPort::HardwareControl));
    m_flowControlCombo->setFixedWidth(100);
    m_connectButton = new QPushButton(QStringLiteral("连接"));
    m_connectButton->setObjectName(QStringLiteral("connectButton"));
    connectionLayout->addWidget(new QLabel(QStringLiteral("端口")), 0, 0);
    connectionLayout->addWidget(m_portCombo, 0, 1, Qt::AlignLeft);
    connectionLayout->addWidget(new QLabel(QStringLiteral("波特率")), 0, 2);
    connectionLayout->addWidget(m_baudCombo, 0, 3, Qt::AlignLeft);
    connectionLayout->addWidget(new QLabel(QStringLiteral("数据位")), 1, 0);
    connectionLayout->addWidget(m_dataBitsCombo, 1, 1, Qt::AlignLeft);
    connectionLayout->addWidget(new QLabel(QStringLiteral("校验")), 1, 2);
    connectionLayout->addWidget(m_parityCombo, 1, 3, Qt::AlignLeft);
    connectionLayout->addWidget(new QLabel(QStringLiteral("停止")), 2, 0);
    connectionLayout->addWidget(m_stopBitsCombo, 2, 1, Qt::AlignLeft);
    connectionLayout->addWidget(new QLabel(QStringLiteral("流控")), 2, 2);
    connectionLayout->addWidget(m_flowControlCombo, 2, 3, Qt::AlignLeft);
    connectionLayout->addWidget(m_refreshPortsButton, 3, 0, 1, 2);
    connectionLayout->addWidget(m_connectButton, 3, 2, 1, 2);
    connectionLayout->setColumnStretch(1, 1);
    connectionLayout->setColumnStretch(3, 1);
    sidebarLayout->addWidget(connectionGroup);

    auto *controlGroup = new QGroupBox(QStringLiteral("电机控制"));
    auto *controlLayout = new QVBoxLayout(controlGroup);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    controlLayout->setSpacing(5);

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("控制模式")));
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    m_positionModeCheck = new QCheckBox(QStringLiteral("位置"));
    m_velocityModeCheck = new QCheckBox(QStringLiteral("速度"));
    m_currentModeCheck = new QCheckBox(QStringLiteral("电流"));
    m_modeGroup->addButton(m_positionModeCheck, 0);
    m_modeGroup->addButton(m_velocityModeCheck, 1);
    m_modeGroup->addButton(m_currentModeCheck, 2);
    m_positionModeCheck->setChecked(true);
    modeRow->addWidget(m_positionModeCheck);
    modeRow->addWidget(m_velocityModeCheck);
    modeRow->addWidget(m_currentModeCheck);
    modeRow->addStretch();
    controlLayout->addLayout(modeRow);

    auto *targetForm = new QGridLayout;
    m_targetSpin = new QDoubleSpinBox;
    m_targetSpin->setDecimals(3);
    m_targetSpin->setRange(-41943.040, 41943.039);
    m_targetSpin->setKeyboardTracking(false);
    m_targetSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    m_targetUnitLabel = new QLabel(QStringLiteral("圈"));
    auto *sendTargetButton = new QPushButton(QStringLiteral("下发目标"));
    sendTargetButton->setObjectName(QStringLiteral("primaryButton"));
    targetForm->addWidget(new QLabel(QStringLiteral("目标值")), 0, 0);
    targetForm->addWidget(m_targetSpin, 0, 1);
    targetForm->addWidget(m_targetUnitLabel, 0, 2);
    targetForm->addWidget(sendTargetButton, 1, 0, 1, 3);
    controlLayout->addLayout(targetForm);

    auto *modeHint = new QLabel(QStringLiteral(
        "位置单位：圈（最多 3 位小数）\n"
        "速度单位：圈/秒；电流单位：mA\n"
        "速度/电流范围会按驱动上报的配置限幅"));
    modeHint->setObjectName(QStringLiteral("hintLabel"));
    modeHint->setWordWrap(true);
    controlLayout->addWidget(modeHint);

    auto *operationGroup = new QGroupBox(QStringLiteral("实时状态与操作"));
    auto *operationLayout = new QVBoxLayout(operationGroup);
    operationLayout->setContentsMargins(8, 8, 8, 8);
    operationLayout->setSpacing(5);

    auto *statusLayout = new QGridLayout;
    statusLayout->setSpacing(4);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("运行状态"), &m_stateValue), 0, 0);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("功率级"), &m_powerValue), 0, 1);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("位置"), &m_positionValue), 1, 0);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("速度"), &m_velocityValue), 1, 1);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("电流"), &m_currentValue), 2, 0);
    statusLayout->addWidget(makeMetricCard(QStringLiteral("编码器 / 故障"), &m_encoderFaultValue), 2, 1);
    operationLayout->addLayout(statusLayout);

    auto *motionButtons = new QGridLayout;
    motionButtons->setSpacing(4);
    auto *enableButton = new QPushButton(QStringLiteral("使能"));
    enableButton->setObjectName(QStringLiteral("successButton"));
    auto *disableButton = new QPushButton(QStringLiteral("失能 / 停止"));
    disableButton->setObjectName(QStringLiteral("dangerButton"));
    auto *brakeButton = new QPushButton(QStringLiteral("刹车"));
    auto *calibrateButton = new QPushButton(QStringLiteral("编码器校准"));
    auto *homeButton = new QPushButton(QStringLiteral("设当前位置为零点"));
    auto *saveButton = new QPushButton(QStringLiteral("保存配置"));
    homeButton->setObjectName(QStringLiteral("compactButton"));
    saveButton->setObjectName(QStringLiteral("compactButton"));
    motionButtons->addWidget(enableButton, 0, 0);
    motionButtons->addWidget(disableButton, 0, 1);
    motionButtons->addWidget(brakeButton, 1, 0);
    motionButtons->addWidget(calibrateButton, 1, 1);
    motionButtons->addWidget(homeButton, 2, 0);
    motionButtons->addWidget(saveButton, 2, 1);
    operationLayout->addLayout(motionButtons);

    auto *safetyHint = new QLabel(QStringLiteral(
        "校准前确保电机空载、轴可自由转动；失能释放电机轴，刹车会短接线圈。"));
    safetyHint->setObjectName(QStringLiteral("warningLabel"));
    safetyHint->setWordWrap(true);
    operationLayout->addWidget(safetyHint);

    auto *controlScroll = new QScrollArea;
    controlScroll->setWidgetResizable(true);
    controlScroll->setFrameShape(QFrame::NoFrame);
    controlScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlScroll->setMinimumWidth(318);
    sidebarLayout->addWidget(controlGroup);
    sidebarLayout->addWidget(operationGroup);

    auto *diagnosticsGroup = new QGroupBox(QStringLiteral("实时诊断"));
    auto *diagnosticsLayout = new QVBoxLayout(diagnosticsGroup);
    diagnosticsLayout->setContentsMargins(8, 8, 8, 8);
    diagnosticsLayout->setSpacing(5);
    const auto addDiagnosticSection =
        [diagnosticsLayout](const QString &title, const QStringList &rowTitles) {
            auto *section = new QWidget;
            auto *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setContentsMargins(0, 0, 0, 0);
            sectionLayout->setSpacing(2);
            auto *header = new QToolButton;
            header->setObjectName(QStringLiteral("diagnosticSectionHeader"));
            header->setText(title);
            header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            header->setArrowType(Qt::DownArrow);
            header->setCheckable(true);
            header->setChecked(true);
            header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            auto *content = new QWidget;
            content->setObjectName(QStringLiteral("diagnosticSectionContent"));
            auto *grid = new QGridLayout(content);
            grid->setContentsMargins(6, 4, 4, 4);
            grid->setHorizontalSpacing(5);
            grid->setVerticalSpacing(2);
            QVector<QLabel *> values;
            for (int row = 0; row < rowTitles.size(); ++row)
            {
                auto *key = new QLabel(rowTitles.at(row));
                key->setObjectName(QStringLiteral("diagnosticKey"));
                auto *value = new QLabel(QStringLiteral("--"));
                value->setObjectName(QStringLiteral("diagnosticValue"));
                value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                value->setTextInteractionFlags(Qt::TextSelectableByMouse);
                grid->addWidget(key, row, 0);
                grid->addWidget(value, row, 1);
                values.append(value);
            }
            grid->setColumnStretch(1, 1);
            sectionLayout->addWidget(header);
            sectionLayout->addWidget(content);
            QObject::connect(header, &QToolButton::toggled, content,
                             [header, content](bool expanded) {
                                 content->setVisible(expanded);
                                 header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
                             });
            diagnosticsLayout->addWidget(section);
            return values;
        };
    const QVector<QLabel *> communicationValues =
        addDiagnosticSection(QStringLiteral("通信与系统负载"),
                             {QStringLiteral("ISR 最大耗时："),
                              QStringLiteral("ISR 超时 / 警告："),
                              QStringLiteral("UART 丢字节 / 错误："),
                              QStringLiteral("SPI 错误 / 状态：")});
    m_isrMaxValue = communicationValues.at(0);
    m_isrWarningValue = communicationValues.at(1);
    m_uartValue = communicationValues.at(2);
    m_spiValue = communicationValues.at(3);

    const QVector<QLabel *> controllerValues =
        addDiagnosticSection(QStringLiteral("控制器与驱动参数"),
                             {QStringLiteral("控制器 (tgt / est)："),
                              QStringLiteral("DCE 参数 (kp / kv / ki / kd)："),
                              QStringLiteral("DCE 输出 (ki / out)："),
                              QStringLiteral("CAN 状态："),
                              QStringLiteral("配置上限 (电流 / 速度)：")});
    m_controllerValue = controllerValues.at(0);
    m_dceGainsValue = controllerValues.at(1);
    m_dceOutputValue = controllerValues.at(2);
    m_canValue = controllerValues.at(3);
    m_limitsValue = controllerValues.at(4);

    m_lastUpdateLabel = new QLabel(QStringLiteral("等待连接"));
    m_lastUpdateLabel->setObjectName(QStringLiteral("diagnosticsTimestamp"));
    m_lastUpdateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    diagnosticsLayout->addWidget(m_lastUpdateLabel);
    sidebarLayout->addWidget(diagnosticsGroup);
    sidebarLayout->addStretch();

    controlScroll->setWidget(sidebarContent);

    auto *rightPanel = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto *chartGroup = new QGroupBox(QStringLiteral("实时曲线（可拖动滚动条查看历史）"));
    auto *chartLayout = new QVBoxLayout(chartGroup);
    auto *chartHeader = new QHBoxLayout;
    auto *chartHint = new QLabel(QStringLiteral("历史保留：5 分钟 · 鼠标滚轮缩放"));
    chartHint->setObjectName(QStringLiteral("hintLabel"));
    chartHeader->addStretch();
    chartHint->setToolTip(QStringLiteral("滚轮缩放；下方滚动条可回看最近 5 分钟"));
    chartHeader->addWidget(chartHint);
    chartLayout->addLayout(chartHeader);

    auto *plotsColumn = new QVBoxLayout;
    m_positionPlot = new TimeSeriesPlot(QStringLiteral("位置"), QStringLiteral("圈"),
                                        QColor(QStringLiteral("#60a5fa")));
    m_velocityPlot = new TimeSeriesPlot(QStringLiteral("速度"), QStringLiteral("圈/秒"),
                                        QColor(QStringLiteral("#34d399")));
    m_currentPlot = new TimeSeriesPlot(QStringLiteral("电流"), QStringLiteral("mA"),
                                       QColor(QStringLiteral("#fbbf24")));
    plotsColumn->addWidget(m_positionPlot);
    plotsColumn->addWidget(m_velocityPlot);
    plotsColumn->addWidget(m_currentPlot);
    chartLayout->addLayout(plotsColumn, 1);
    rightLayout->addWidget(chartGroup, 1);

    mainSplitter->addWidget(controlScroll);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({318, 1002});
    root->addWidget(mainSplitter, 1);

    auto *consoleGroup = new QGroupBox(QStringLiteral("串口收发日志（仅允许发送 xdrv 命令）"));
    auto *consoleLayout = new QVBoxLayout(consoleGroup);
    m_consoleOutput = new QPlainTextEdit;
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setMaximumBlockCount(1200);
    m_consoleOutput->setFixedHeight(72);
    auto *consoleRow = new QHBoxLayout;
    m_consoleInput = new QLineEdit;
    m_consoleInput->setPlaceholderText(QStringLiteral("例如：xdrv status 或 xdrv pos 1000"));
    auto *sendConsoleButton = new QPushButton(QStringLiteral("发送"));
    consoleRow->addWidget(m_consoleInput, 1);
    consoleRow->addWidget(sendConsoleButton);
    consoleLayout->addWidget(m_consoleOutput);
    consoleLayout->addLayout(consoleRow);
    rightLayout->addWidget(consoleGroup);

    setCentralWidget(central);
    QTimer::singleShot(0, controlScroll,
                       [controlScroll] { controlScroll->verticalScrollBar()->setValue(0); });

    connect(m_refreshPortsButton, &QPushButton::clicked, this, [this] { refreshPorts(); });
    connect(m_connectButton, &QPushButton::clicked, this, [this] { toggleConnection(); });
    connect(sendTargetButton, &QPushButton::clicked, this, [this] { sendMotionTarget(); });
    connect(m_positionModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            updateTargetMode();
    });
    connect(m_velocityModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            updateTargetMode();
    });
    connect(m_currentModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            updateTargetMode();
    });
    connect(enableButton, &QPushButton::clicked, this, [this] { queueCommand(QStringLiteral("xdrv on")); });
    connect(disableButton, &QPushButton::clicked, this, [this] { queueCommand(QStringLiteral("xdrv off")); });
    connect(brakeButton, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("确认刹车"),
            QStringLiteral("刹车会短接两相线圈，电机轴会变硬。确定发送刹车命令吗？"));
        if (answer == QMessageBox::Yes)
            queueCommand(QStringLiteral("xdrv brake"));
    });
    connect(calibrateButton, &QPushButton::clicked, this, [this] {
        confirmAndQueueSequence(
            QStringLiteral("确认编码器校准"),
            QStringLiteral("校准会强制驱动电机转动约两圈。请确认电机空载、轴可自由转动且周围没有障碍物。\n\n"
                           "程序将先发送失能命令，再启动校准。"),
            {QStringLiteral("xdrv off"), QStringLiteral("xdrv cali")});
    });
    connect(homeButton, &QPushButton::clicked, this, [this] {
        confirmAndQueueSequence(
            QStringLiteral("确认设置零点"),
            QStringLiteral("将先失能电机，再把当前位置设为零点并写入配置。继续吗？"),
            {QStringLiteral("xdrv off"), QStringLiteral("xdrv home")});
    });
    connect(saveButton, &QPushButton::clicked, this, [this] {
        confirmAndQueueSequence(
            QStringLiteral("确认保存配置"),
            QStringLiteral("将先失能电机，再保存当前配置到 Flash。继续吗？"),
            {QStringLiteral("xdrv off"), QStringLiteral("xdrv save")});
    });
    connect(sendConsoleButton, &QPushButton::clicked, this, [this] {
        const QString command = m_consoleInput->text().trimmed();
        if (command.isEmpty())
            return;
        if (!command.startsWith(QStringLiteral("xdrv"), Qt::CaseInsensitive)
            || (command.size() > 4 && !command.at(4).isSpace()))
        {
            QMessageBox::warning(this, QStringLiteral("命令受限"),
                                 QStringLiteral("为避免误操作，控制台仅接受 xdrv 命令。"));
            return;
        }
        m_consoleInput->clear();
        queueCommand(command);
    });
    connect(m_consoleInput, &QLineEdit::returnPressed, sendConsoleButton, &QPushButton::click);

    updateTargetMode();
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget { background: #202020; color: #dddddd; font-family: "Segoe UI", "Microsoft YaHei UI"; font-size: 9pt; }
        QMainWindow { background: #1b1b1b; }
        QGroupBox { background: #252525; border: 1px solid #3b3b3b; border-radius: 3px; margin-top: 11px; padding: 5px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #9a9a9a; }
        QFrame#metricCard { background: #202020; border: 1px solid #414141; border-radius: 3px; }
        QLabel#metricTitle, QLabel#hintLabel { color: #9b9b9b; }
        QLabel#metricValue { color: #f2f2f2; font-size: 9pt; font-weight: 600; }
        QLabel#warningLabel { color: #e1c27a; background: #29251d; border: 1px solid #49402d; border-radius: 3px; padding: 4px; }
        QLabel#diagnosticKey { color: #b6b6b6; font-size: 8pt; }
        QLabel#diagnosticValue { color: #e0e0e0; font-family: Consolas, monospace; font-size: 7pt; }
        QLabel#diagnosticsTimestamp { color: #888888; font-size: 7pt; }
        QToolButton#diagnosticSectionHeader { background: #252525; color: #aaaaaa; border: none; padding: 2px; text-align: left; }
        QWidget#diagnosticSectionContent { background: #202020; border-top: 1px solid #3b3b3b; }
        QComboBox, QDoubleSpinBox, QLineEdit { background: #303030; border: 1px solid #484848; border-radius: 3px; padding: 4px; selection-background-color: #167bc0; }
        QComboBox:disabled, QDoubleSpinBox:disabled, QLineEdit:disabled { color: #777777; }
        QDoubleSpinBox { min-height: 34px; padding-right: 32px; }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 28px; border-left: 1px solid #484848; }
        QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; }
        QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; }
        QCheckBox { spacing: 5px; color: #dddddd; }
        QCheckBox::indicator { width: 13px; height: 13px; background: #303030; border: 1px solid #777777; border-radius: 7px; }
        QCheckBox::indicator:checked { background: #1687c9; border-color: #55b4ed; }
        QWidget#timeSeriesPlot { border: 1px solid #383838; border-radius: 3px; }
        QScrollBar:vertical { background: #303030; width: 11px; margin: 0; }
        QScrollBar::handle:vertical { background: #777777; min-height: 20px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #292929; }
        QScrollBar#plotScrollBar:horizontal { background: #161616; border: none; height: 8px; margin: 0 3px; }
        QScrollBar#plotScrollBar::handle:horizontal { background: #555555; min-width: 22px; border-radius: 3px; }
        QScrollBar#plotScrollBar::handle:horizontal:hover { background: #777777; }
        QScrollBar#plotScrollBar:disabled { background: #161616; }
        QScrollBar#plotScrollBar::handle:horizontal:disabled { background: #282828; }
        QScrollBar#plotScrollBar::add-line:horizontal, QScrollBar#plotScrollBar::sub-line:horizontal { width: 0; border: none; }
        QScrollBar#plotScrollBar::add-page:horizontal, QScrollBar#plotScrollBar::sub-page:horizontal { background: #161616; }
        QPushButton { background: #454545; border: 1px solid #555555; border-radius: 3px; padding: 4px 7px; }
        QPushButton:hover { background: #555555; }
        QPushButton:disabled { color: #777777; background: #303030; }
        QPushButton#primaryButton { background: #087dc3; border-color: #0b8bd7; color: white; font-weight: 600; }
        QPushButton#primaryButton:hover { background: #0a8bd5; }
        QPushButton#connectButton, QPushButton#successButton { background: #267c48; border-color: #318d55; color: white; font-weight: 600; }
        QPushButton#connectButton:hover, QPushButton#successButton:hover { background: #319456; }
        QPushButton#dangerButton { background: #e74848; border-color: #f05a5a; color: white; font-weight: 600; }
        QPushButton#dangerButton:hover { background: #f05252; }
        QPushButton#compactButton { font-size: 8pt; }
        QPlainTextEdit { background: #050505; color: #31d34b; border: 1px solid #383838; border-radius: 3px; padding: 5px; font-family: Consolas, monospace; font-size: 8pt; }
        QSplitter::handle { background: #1b1b1b; width: 4px; }
    )"));
}

void MainWindow::loadSerialSettings()
{
    QSettings settings;
    m_portCombo->setEditText(settings.value(QStringLiteral("serial/port")).toString());
    m_baudCombo->setCurrentText(settings.value(QStringLiteral("serial/baudRate"), 115200).toString());
    selectComboData(m_dataBitsCombo,
                    settings.value(QStringLiteral("serial/dataBits"),
                                   static_cast<int>(QSerialPort::Data8)).toInt());
    selectComboData(m_parityCombo,
                    settings.value(QStringLiteral("serial/parity"),
                                   static_cast<int>(QSerialPort::NoParity)).toInt());
    selectComboData(m_stopBitsCombo,
                    settings.value(QStringLiteral("serial/stopBits"),
                                   static_cast<int>(QSerialPort::OneStop)).toInt());
    selectComboData(m_flowControlCombo,
                    settings.value(QStringLiteral("serial/flowControl"),
                                   static_cast<int>(QSerialPort::NoFlowControl)).toInt());
}

void MainWindow::saveSerialSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("serial/port"), selectedPortName());
    settings.setValue(QStringLiteral("serial/baudRate"), m_baudCombo->currentText().trimmed());
    settings.setValue(QStringLiteral("serial/dataBits"), m_dataBitsCombo->currentData());
    settings.setValue(QStringLiteral("serial/parity"), m_parityCombo->currentData());
    settings.setValue(QStringLiteral("serial/stopBits"), m_stopBitsCombo->currentData());
    settings.setValue(QStringLiteral("serial/flowControl"), m_flowControlCombo->currentData());
}

QString MainWindow::selectedPortName() const
{
    const int index = m_portCombo->currentIndex();
    if (index >= 0 && m_portCombo->currentText() == m_portCombo->itemText(index))
        return m_portCombo->currentData().toString();
    return m_portCombo->currentText().trimmed();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSerialSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::refreshPorts()
{
    const QString previousPort = selectedPortName();
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports)
    {
        m_portCombo->addItem(port.portName(), port.portName());
        if (!port.description().isEmpty())
            m_portCombo->setItemData(m_portCombo->count() - 1, port.description(), Qt::ToolTipRole);
    }

    const int previousIndex = m_portCombo->findData(previousPort);
    if (previousIndex >= 0)
        m_portCombo->setCurrentIndex(previousIndex);
    else
        m_portCombo->setEditText(previousPort);
}

void MainWindow::toggleConnection()
{
    saveSerialSettings();
    if (m_serial->isOpen())
    {
        closeSerialPort();
        return;
    }

    const QString portName = selectedPortName();
    if (portName.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("无法连接"), QStringLiteral("请先选择可用串口。"));
        return;
    }

    bool baudOk = false;
    const int baudRate = m_baudCombo->currentText().toInt(&baudOk);
    if (!baudOk || baudRate <= 0)
    {
        QMessageBox::warning(this, QStringLiteral("波特率无效"), QStringLiteral("请输入有效的波特率。"));
        return;
    }

    m_serial->setPortName(portName);
    const bool settingsOk =
        m_serial->setBaudRate(baudRate)
        && m_serial->setDataBits(static_cast<QSerialPort::DataBits>(m_dataBitsCombo->currentData().toInt()))
        && m_serial->setParity(static_cast<QSerialPort::Parity>(m_parityCombo->currentData().toInt()))
        && m_serial->setStopBits(static_cast<QSerialPort::StopBits>(m_stopBitsCombo->currentData().toInt()))
        && m_serial->setFlowControl(static_cast<QSerialPort::FlowControl>(m_flowControlCombo->currentData().toInt()));
    if (!settingsOk)
    {
        QMessageBox::critical(this, QStringLiteral("串口参数无效"), m_serial->errorString());
        return;
    }

    if (!m_serial->open(QIODevice::ReadWrite))
    {
        QMessageBox::critical(this, QStringLiteral("串口打开失败"), m_serial->errorString());
        updateConnectionUi();
        return;
    }

    m_receiveBuffer.clear();
    m_statusParser.reset();
    m_commandQueue.clear();
    m_statusRequestPending = false;
    clearChartHistory();
    updateConnectionUi();
    appendLog(QStringLiteral("SYS"),
              QStringLiteral("已连接 %1，%2 %3%4%5，流控：%6")
                  .arg(portName, QString::number(baudRate),
                       m_dataBitsCombo->currentText(),
                       m_parityCombo->currentText(),
                       m_stopBitsCombo->currentText(),
                       m_flowControlCombo->currentText()));
    m_pollTimer.start();
    queueCommand(QStringLiteral("xdrv status"));
}

void MainWindow::closeSerialPort()
{
    m_pollTimer.stop();
    m_statusTimeout.stop();
    m_commandTimer.stop();
    m_commandQueue.clear();
    m_statusRequestPending = false;
    if (m_serial->isOpen())
        m_serial->close();
    m_statusParser.reset();
    m_receiveBuffer.clear();
    clearChartHistory();
    updateConnectionUi();
    m_lastUpdateLabel->setText(QStringLiteral("等待连接"));
    m_isrMaxValue->setText(QStringLiteral("--"));
    m_isrWarningValue->setText(QStringLiteral("--"));
    m_uartValue->setText(QStringLiteral("--"));
    m_spiValue->setText(QStringLiteral("--"));
    m_controllerValue->setText(QStringLiteral("--"));
    m_dceGainsValue->setText(QStringLiteral("--"));
    m_dceOutputValue->setText(QStringLiteral("--"));
    m_canValue->setText(QStringLiteral("--"));
    m_limitsValue->setText(QStringLiteral("--"));
    m_stateValue->setText(QStringLiteral("--"));
    m_powerValue->setText(QStringLiteral("--"));
    m_positionValue->setText(QStringLiteral("--"));
    m_velocityValue->setText(QStringLiteral("--"));
    m_currentValue->setText(QStringLiteral("--"));
    m_encoderFaultValue->setText(QStringLiteral("--"));
    m_encoderFaultValue->setStyleSheet(QString());
}

void MainWindow::updateConnectionUi()
{
    const bool connected = m_serial->isOpen();
    m_connectButton->setText(connected ? QStringLiteral("断开（已连接）")
                                      : QStringLiteral("连接"));
    m_portCombo->setEnabled(!connected);
    m_refreshPortsButton->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_dataBitsCombo->setEnabled(!connected);
    m_parityCombo->setEnabled(!connected);
    m_stopBitsCombo->setEnabled(!connected);
    m_flowControlCombo->setEnabled(!connected);
}

void MainWindow::readSerialData()
{
    m_receiveBuffer.append(m_serial->readAll());
    int newlineIndex = -1;
    while ((newlineIndex = m_receiveBuffer.indexOf('\n')) >= 0)
    {
        QByteArray line = m_receiveBuffer.left(newlineIndex);
        m_receiveBuffer.remove(0, newlineIndex + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        processLine(QString::fromLocal8Bit(line));
    }

    if (m_receiveBuffer.size() > 8192)
    {
        appendLog(QStringLiteral("SYS"), QStringLiteral("丢弃过长的串口行（超过 8192 字节）"));
        m_receiveBuffer.clear();
    }
}

void MainWindow::processLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    const bool statusStart = trimmed.startsWith(QStringLiteral("state"), Qt::CaseInsensitive)
                             && trimmed.contains(QLatin1Char(':'));
    const bool statusLine = m_statusParser.isCollectingSnapshot() || statusStart;
    const bool statusEcho = trimmed.compare(QStringLiteral("xdrv status"), Qt::CaseInsensitive) == 0
                            || trimmed.startsWith(QStringLiteral("msh >"), Qt::CaseInsensitive);
    const bool snapshotComplete = m_statusParser.consumeLine(line);

    if (!line.isEmpty() && !statusLine && !statusEcho)
        appendLog(QStringLiteral("RX"), line);
    if (snapshotComplete)
    {
        m_statusRequestPending = false;
        m_statusTimeout.stop();
        updateStatusUi(m_statusParser.status());
        m_lastUpdateLabel->setText(QStringLiteral("最近更新：%1")
                                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
        m_lastUpdateLabel->setStyleSheet(QStringLiteral("color: #9a9a9a;"));
    }
}

void MainWindow::queueCommand(const QString &command)
{
    if (!m_serial->isOpen())
    {
        QMessageBox::information(this, QStringLiteral("尚未连接"), QStringLiteral("请先连接驱动串口。"));
        return;
    }

    m_commandQueue.enqueue(command);
    if (!m_commandTimer.isActive())
        m_commandTimer.start(0);
}

void MainWindow::dispatchNextCommand()
{
    if (m_commandQueue.isEmpty() || !m_serial->isOpen())
        return;

    const QString command = m_commandQueue.dequeue();
    const QByteArray bytes = command.toUtf8() + QByteArrayLiteral("\r");
    const qint64 accepted = m_serial->write(bytes);
    if (accepted != bytes.size())
    {
        appendLog(QStringLiteral("ERR"), QStringLiteral("发送失败：%1").arg(m_serial->errorString()));
    }
    else
    {
        if (command == QStringLiteral("xdrv status"))
        {
            m_statusRequestPending = true;
            m_statusTimeout.start(2500);
        }
        else
        {
            appendLog(QStringLiteral("TX"), command);
        }
    }

    if (!m_commandQueue.isEmpty())
        m_commandTimer.start(150);
}

void MainWindow::sendMotionTarget()
{
    const QString mode = selectedMotionMode();
    qint64 rawValue = 0;
    if (mode == QStringLiteral("pos"))
    {
        rawValue = qRound64(m_targetSpin->value() * 1000.0);
    }
    else if (mode == QStringLiteral("vel"))
    {
        rawValue = qRound64(m_targetSpin->value() * 1000.0);
    }
    else
    {
        rawValue = qRound64(m_targetSpin->value());
    }

    if (rawValue < std::numeric_limits<qint32>::min()
        || rawValue > std::numeric_limits<qint32>::max())
    {
        QMessageBox::warning(this, QStringLiteral("目标值超范围"), QStringLiteral("目标值超出固件命令可表示范围。"));
        return;
    }

    queueCommand(QStringLiteral("xdrv %1 %2").arg(mode).arg(rawValue));
}

void MainWindow::confirmAndQueueSequence(const QString &title, const QString &message,
                                         const QStringList &commands)
{
    if (!m_serial->isOpen())
    {
        QMessageBox::information(this, QStringLiteral("尚未连接"), QStringLiteral("请先连接驱动串口。"));
        return;
    }

    if (QMessageBox::warning(this, title, message, QMessageBox::Yes | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    for (const QString &command : commands)
        queueCommand(command);
}

void MainWindow::pollStatus()
{
    if (m_serial->isOpen() && !m_statusRequestPending)
        queueCommand(QStringLiteral("xdrv status"));
}

void MainWindow::updateStatusUi(const XdrvStatus &status)
{
    const qint64 sampleTime = QDateTime::currentMSecsSinceEpoch();
    m_stateValue->setText(printableOrDash(status.state));
    m_powerValue->setText(printableOrDash(status.power));
    m_positionValue->setText(status.positionMilliRev
                                  ? QStringLiteral("%1 圈").arg(*status.positionMilliRev / 1000.0, 0, 'f', 3)
                                  : QStringLiteral("--"));
    m_velocityValue->setText(status.velocityMilliRevPerSec
                                 ? QStringLiteral("%1 圈/秒").arg(*status.velocityMilliRevPerSec / 1000.0, 0, 'f', 3)
                                 : QStringLiteral("--"));
    m_currentValue->setText(status.currentMilliamp
                                ? QStringLiteral("%1 mA").arg(*status.currentMilliamp)
                                : QStringLiteral("--"));
    QStringList encoderFaultState;
    if (status.calibrated)
        encoderFaultState << (*status.calibrated ? QStringLiteral("已校准")
                                                 : QStringLiteral("未校准"));
    if (status.stalled || status.overloaded)
    {
        const int stalled = status.stalled.value_or(0);
        const int overloaded = status.overloaded.value_or(0);
        encoderFaultState << ((stalled || overloaded)
                                  ? QStringLiteral("堵转 %1 / 过载 %2").arg(stalled).arg(overloaded)
                                  : QStringLiteral("正常"));
        m_encoderFaultValue->setStyleSheet((stalled || overloaded)
                                               ? QStringLiteral("color:#ff7070;")
                                               : QStringLiteral("color:#42d36b;"));
    }
    else
    {
        m_encoderFaultValue->setStyleSheet(QString());
    }
    m_encoderFaultValue->setText(encoderFaultState.isEmpty()
                                     ? QStringLiteral("--")
                                     : encoderFaultState.join(QStringLiteral(" · ")));

    m_isrMaxValue->setText(status.isrMaxMicroseconds
                               ? QStringLiteral("%1 µs").arg(*status.isrMaxMicroseconds, 0, 'f', 2)
                               : QStringLiteral("--"));
    m_isrWarningValue->setText(status.isrWarning.isEmpty()
                                   ? QStringLiteral("无")
                                   : status.isrWarning);
    m_uartValue->setText(
        QStringLiteral("%1 / %2")
            .arg(status.uartOverruns ? QString::number(*status.uartOverruns) : QStringLiteral("--"),
                 status.uartErrors ? QString::number(*status.uartErrors) : QStringLiteral("--")));

    const QString spiErrorCount =
        status.spiErrors ? QString::number(*status.spiErrors) : QStringLiteral("--");
    const QString spiState = status.spiFailure.isEmpty() ? QStringLiteral("正常")
                                                         : status.spiFailure;
    m_spiValue->setText(QStringLiteral("%1 / %2").arg(spiErrorCount, spiState));

    static const QRegularExpression controllerPattern(
        QStringLiteral("tgt=(-?\\d+)\\s+est=(-?\\d+)\\s+err=(-?\\d+)"));
    const auto controllerMatch = controllerPattern.match(status.controller);
    m_controllerValue->setText(
        controllerMatch.hasMatch()
            ? QStringLiteral("%1 / %2 (err %3)")
                  .arg(controllerMatch.captured(1), controllerMatch.captured(2),
                       controllerMatch.captured(3))
            : printableOrDash(status.controller));

    static const QRegularExpression dcePattern(
        QStringLiteral("ki=(-?\\d+)\\s+out=(-?\\d+)\\s+\\(kp=(-?\\d+)\\s+kv=(-?\\d+)\\s+ki=(-?\\d+)\\s+kd=(-?\\d+)\\)"));
    const auto dceMatch = dcePattern.match(status.dce);
    m_dceGainsValue->setText(
        dceMatch.hasMatch()
            ? QStringLiteral("%1 / %2 / %3 / %4")
                  .arg(dceMatch.captured(3), dceMatch.captured(4),
                       dceMatch.captured(5), dceMatch.captured(6))
            : printableOrDash(status.dce));
    m_dceOutputValue->setText(
        dceMatch.hasMatch()
            ? QStringLiteral("%1 / %2").arg(dceMatch.captured(1), dceMatch.captured(2))
            : QStringLiteral("--"));
    m_canValue->setText(printableOrDash(status.can));

    const QString currentLimit = status.currentLimitMilliamp
                                     ? QStringLiteral("%1 mA").arg(*status.currentLimitMilliamp)
                                     : QStringLiteral("--");
    const QString velocityLimit =
        status.velocityLimitRevPerSec
            ? QStringLiteral("%1 圈/秒").arg(*status.velocityLimitRevPerSec, 0, 'f', 3)
            : QStringLiteral("--");
    m_limitsValue->setText(QStringLiteral("%1 / %2").arg(currentLimit, velocityLimit));

    if (status.positionMilliRev)
        m_positionPlot->appendSample(sampleTime, *status.positionMilliRev / 1000.0);
    if (status.velocityMilliRevPerSec)
        m_velocityPlot->appendSample(sampleTime, *status.velocityMilliRevPerSec / 1000.0);
    if (status.currentMilliamp)
        m_currentPlot->appendSample(sampleTime, *status.currentMilliamp);

    if (selectedMotionMode() == QStringLiteral("vel"))
    {
        const double limit = status.velocityLimitRevPerSec.value_or(30.0);
        m_targetSpin->setRange(-qMin(limit, 200.0), qMin(limit, 200.0));
    }
    else if (selectedMotionMode() == QStringLiteral("cur"))
    {
        const double limit = status.currentLimitMilliamp.value_or(1000);
        m_targetSpin->setRange(-limit, limit);
    }
}

void MainWindow::updateTargetMode()
{
    const QString mode = selectedMotionMode();
    if (mode == QStringLiteral("pos"))
    {
        m_targetSpin->setRange(-41943.040, 41943.039);
        m_targetSpin->setDecimals(3);
        m_targetUnitLabel->setText(QStringLiteral("圈"));
    }
    else if (mode == QStringLiteral("vel"))
    {
        const double limit = m_statusParser.status().velocityLimitRevPerSec.value_or(30.0);
        m_targetSpin->setRange(-qMin(limit, 200.0), qMin(limit, 200.0));
        m_targetSpin->setDecimals(3);
        m_targetUnitLabel->setText(QStringLiteral("圈/秒"));
    }
    else
    {
        const double limit = m_statusParser.status().currentLimitMilliamp.value_or(1000);
        m_targetSpin->setRange(-limit, limit);
        m_targetSpin->setDecimals(0);
        m_targetUnitLabel->setText(QStringLiteral("mA"));
    }
    m_targetSpin->setValue(0.0);
}

QString MainWindow::selectedMotionMode() const
{
    switch (m_modeGroup->checkedId())
    {
    case 1:
        return QStringLiteral("vel");
    case 2:
        return QStringLiteral("cur");
    default:
        return QStringLiteral("pos");
    }
}

void MainWindow::clearChartHistory()
{
    m_positionPlot->clear();
    m_velocityPlot->clear();
    m_currentPlot->clear();
}

void MainWindow::appendLog(const QString &direction, const QString &text)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_consoleOutput->appendPlainText(QStringLiteral("[%1] [%2] %3").arg(timestamp, direction, text));
}

void MainWindow::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || !m_serial->isOpen())
        return;

    appendLog(QStringLiteral("ERR"), m_serial->errorString());
    if (error == QSerialPort::ResourceError)
    {
        m_pollTimer.stop();
        m_statusTimeout.stop();
        m_commandTimer.stop();
        m_commandQueue.clear();
        m_serial->close();
        updateConnectionUi();
        QMessageBox::critical(this, QStringLiteral("串口连接中断"), QStringLiteral("设备连接已断开：%1")
                                                                          .arg(m_serial->errorString()));
    }
}
