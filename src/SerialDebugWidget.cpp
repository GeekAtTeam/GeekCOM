#include "SerialDebugWidget.h"
#include "SerialManager.h"
#include "HexUtils.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QScrollBar>
#include <QMessageBox>
#include <QSerialPort>
#include <QFrame>

SerialDebugWidget::SerialDebugWidget(SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_serial(serial)
    , m_autoSendTimer(new QTimer(this))
{
    setupUi();
    setupConnections();
    applyConnectedState(false);
}

void SerialDebugWidget::setupUi()
{
    // ===================== Right Config Panel =====================
    auto *rightPanel = new QWidget;
    rightPanel->setFixedWidth(280);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    // -- Port Config Group --
    auto *portGroup = new QGroupBox("串口配置");
    auto *portGrid = new QGridLayout(portGroup);
    portGrid->setSpacing(6);

    portGrid->addWidget(new QLabel("端口"), 0, 0);
    m_portCombo = new QComboBox;
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_refreshPortBtn = new QPushButton("↻");
    m_refreshPortBtn->setFixedWidth(28);
    m_refreshPortBtn->setToolTip("刷新串口列表");
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_portCombo);
    portRow->addWidget(m_refreshPortBtn);
    portGrid->addLayout(portRow, 0, 1);

    portGrid->addWidget(new QLabel("波特率"), 1, 0);
    m_baudCombo = new QComboBox;
    for (auto b : {1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600})
        m_baudCombo->addItem(QString::number(b), b);
    m_baudCombo->setCurrentText("115200");
    portGrid->addWidget(m_baudCombo, 1, 1);

    portGrid->addWidget(new QLabel("校验位"), 2, 0);
    m_parityCombo = new QComboBox;
    m_parityCombo->addItem("None",  QSerialPort::NoParity);
    m_parityCombo->addItem("Odd",   QSerialPort::OddParity);
    m_parityCombo->addItem("Even",  QSerialPort::EvenParity);
    portGrid->addWidget(m_parityCombo, 2, 1);

    portGrid->addWidget(new QLabel("数据位"), 3, 0);
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem("5", QSerialPort::Data5);
    m_dataBitsCombo->addItem("6", QSerialPort::Data6);
    m_dataBitsCombo->addItem("7", QSerialPort::Data7);
    m_dataBitsCombo->addItem("8", QSerialPort::Data8);
    m_dataBitsCombo->setCurrentText("8");
    portGrid->addWidget(m_dataBitsCombo, 3, 1);

    portGrid->addWidget(new QLabel("停止位"), 4, 0);
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem("1",   QSerialPort::OneStop);
    m_stopBitsCombo->addItem("1.5", QSerialPort::OneAndHalfStop);
    m_stopBitsCombo->addItem("2",   QSerialPort::TwoStop);
    portGrid->addWidget(m_stopBitsCombo, 4, 1);

    m_connectBtn = new QPushButton("打开串口");
    m_connectBtn->setCheckable(true);
    m_connectBtn->setMinimumHeight(36);
    portGrid->addWidget(m_connectBtn, 5, 0, 1, 2);

    rightLayout->addWidget(portGroup);

    // -- Receive Options Group --
    auto *rxGroup = new QGroupBox("接收设置");
    auto *rxLayout = new QGridLayout(rxGroup);
    rxLayout->setSpacing(6);

    m_rxHexCheck   = new QCheckBox("十六进制显示");
    m_timestampCheck = new QCheckBox("时间戳");
    m_autoClearCheck = new QCheckBox("自动清空");
    m_clearRxBtn   = new QPushButton("清空接收区");
    m_saveRxBtn    = new QPushButton("保存");

    rxLayout->addWidget(m_rxHexCheck,    0, 0);
    rxLayout->addWidget(m_timestampCheck, 0, 1);
    rxLayout->addWidget(m_autoClearCheck, 1, 0);
    rxLayout->addWidget(m_clearRxBtn,    2, 0);
    rxLayout->addWidget(m_saveRxBtn,     2, 1);

    rightLayout->addWidget(rxGroup);

    // -- Send Options Group --
    auto *txGroup = new QGroupBox("发送设置");
    auto *txLayout = new QGridLayout(txGroup);
    txLayout->setSpacing(6);

    m_txHexCheck = new QCheckBox("十六进制发送");
    txLayout->addWidget(m_txHexCheck, 0, 0, 1, 2);

    txLayout->addWidget(new QLabel("文件:"), 1, 0);
    m_filePathEdit = new QLineEdit;
    m_filePathEdit->setPlaceholderText("选择文件...");
    m_filePathEdit->setReadOnly(true);
    m_chooseFileBtn = new QPushButton("选择");
    m_sendFileBtn   = new QPushButton("发送文件");
    txLayout->addWidget(m_filePathEdit, 1, 1);
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_chooseFileBtn);
    fileRow->addWidget(m_sendFileBtn);
    txLayout->addLayout(fileRow, 2, 0, 1, 2);

    m_autoSendCheck = new QCheckBox("自动发送");
    m_autoSendIntervalSpin = new QSpinBox;
    m_autoSendIntervalSpin->setRange(100, 60000);
    m_autoSendIntervalSpin->setValue(1000);
    m_autoSendIntervalSpin->setSuffix(" ms");
    auto *autoRow = new QHBoxLayout;
    autoRow->addWidget(m_autoSendCheck);
    autoRow->addWidget(m_autoSendIntervalSpin);
    txLayout->addLayout(autoRow, 3, 0, 1, 2);

    rightLayout->addWidget(txGroup);
    rightLayout->addStretch();

    // ===================== Left Content =====================
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Splitter: receive (top) + send (bottom)
    auto *splitter = new QSplitter(Qt::Vertical);

    // Receive area
    auto *rxWidget = new QWidget;
    auto *rxVBox = new QVBoxLayout(rxWidget);
    rxVBox->setContentsMargins(4, 4, 4, 4);
    rxVBox->setSpacing(4);
    auto *rxTitle = new QLabel("接收区");
    rxTitle->setStyleSheet("font-weight:bold; color:#555;");
    m_receiveEdit = new QTextEdit;
    m_receiveEdit->setReadOnly(true);
    m_receiveEdit->setFont(QFont("Courier New", 10));
    m_receiveEdit->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    rxVBox->addWidget(rxTitle);
    rxVBox->addWidget(m_receiveEdit);
    splitter->addWidget(rxWidget);

    // Send area
    auto *txWidget = new QWidget;
    auto *txVBox = new QVBoxLayout(txWidget);
    txVBox->setContentsMargins(4, 4, 4, 4);
    txVBox->setSpacing(4);
    auto *txTitle = new QLabel("发送区");
    txTitle->setStyleSheet("font-weight:bold; color:#555;");
    m_sendEdit = new QTextEdit;
    m_sendEdit->setFont(QFont("Courier New", 10));
    m_sendEdit->setPlaceholderText("输入要发送的数据...");
    m_sendEdit->setMaximumHeight(120);
    m_sendBtn = new QPushButton("发 送");
    m_sendBtn->setFixedHeight(36);
    m_sendBtn->setStyleSheet("QPushButton { background:#0078d4; color:white; font-weight:bold; border-radius:4px; }"
                             "QPushButton:hover { background:#106ebe; }"
                             "QPushButton:disabled { background:#aaa; }");
    txVBox->addWidget(txTitle);
    txVBox->addWidget(m_sendEdit);
    txVBox->addWidget(m_sendBtn);
    splitter->addWidget(txWidget);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    leftLayout->addWidget(splitter);

    // ===================== Status Bar =====================
    auto *statusBar = new QFrame;
    statusBar->setFrameShape(QFrame::StyledPanel);
    statusBar->setFixedHeight(32);
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(8, 0, 8, 0);

    m_rxLabel = new QLabel("RX: 0");
    m_txLabel = new QLabel("TX: 0");
    m_countClearBtn = new QPushButton("计数清零");
    m_countClearBtn->setFlat(true);

    m_autoSendLabel = new QLabel("自动发送:");
    m_autoSendCountSpin = new QSpinBox;
    m_autoSendCountSpin->setRange(0, 99999);
    m_autoSendCountSpin->setValue(0);
    m_autoSendCountSpin->setReadOnly(true);
    m_autoSendCountSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_autoSendCountSpin->setFixedWidth(60);

    statusLayout->addWidget(m_rxLabel);
    statusLayout->addWidget(new QLabel("|"));
    statusLayout->addWidget(m_txLabel);
    statusLayout->addWidget(new QLabel("|"));
    statusLayout->addWidget(m_countClearBtn);
    statusLayout->addStretch();
    statusLayout->addWidget(m_autoSendLabel);
    statusLayout->addWidget(m_autoSendCountSpin);

    // ===================== Root Layout =====================
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(0);

    auto *contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(4);
    contentLayout->addWidget(leftWidget, 1);
    contentLayout->addWidget(rightPanel, 0);

    rootLayout->addLayout(contentLayout, 1);
    rootLayout->addWidget(statusBar, 0);

    // Populate ports
    onRefreshPorts();
}

void SerialDebugWidget::setupConnections()
{
    connect(m_connectBtn,    &QPushButton::toggled, this, &SerialDebugWidget::onToggleConnection);
    connect(m_sendBtn,       &QPushButton::clicked, this, &SerialDebugWidget::onSend);
    connect(m_clearRxBtn,    &QPushButton::clicked, this, &SerialDebugWidget::onClearReceive);
    connect(m_saveRxBtn,     &QPushButton::clicked, this, &SerialDebugWidget::onSaveReceive);
    connect(m_chooseFileBtn, &QPushButton::clicked, this, &SerialDebugWidget::onChooseFile);
    connect(m_sendFileBtn,   &QPushButton::clicked, this, &SerialDebugWidget::onSendFile);
    connect(m_countClearBtn, &QPushButton::clicked, this, &SerialDebugWidget::onCountClear);
    connect(m_refreshPortBtn,&QPushButton::clicked, this, &SerialDebugWidget::onRefreshPorts);
    connect(m_autoSendCheck, &QCheckBox::toggled,   this, &SerialDebugWidget::onAutoSendToggle);

    connect(m_autoSendTimer, &QTimer::timeout, this, &SerialDebugWidget::onSend);

    // Update status bar periodically
    auto *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &SerialDebugWidget::updateStatusBar);
    statusTimer->start(500);
}

void SerialDebugWidget::onToggleConnection()
{
    if (m_connectBtn->isChecked()) {
        QString port = m_portCombo->currentText();
        int baud = m_baudCombo->currentData().toInt();
        auto parity   = (QSerialPort::Parity)  m_parityCombo->currentData().toInt();
        auto dataBits = (QSerialPort::DataBits) m_dataBitsCombo->currentData().toInt();
        auto stopBits = (QSerialPort::StopBits) m_stopBitsCombo->currentData().toInt();

        if (!m_serial->open(port, baud, dataBits, parity, stopBits)) {
            m_connectBtn->setChecked(false);
            QMessageBox::warning(this, "连接失败", m_serial->isOpen() ? "" : "无法打开串口，请检查端口设置。");
            return;
        }
        applyConnectedState(true);
    } else {
        m_autoSendTimer->stop();
        m_autoSendCheck->setChecked(false);
        m_serial->close();
        applyConnectedState(false);
    }
}

void SerialDebugWidget::applyConnectedState(bool connected)
{
    m_connectBtn->setText(connected ? "关闭串口" : "打开串口");
    m_connectBtn->setStyleSheet(connected
        ? "QPushButton { background:#c00; color:white; font-weight:bold; border-radius:4px; min-height:36px; }"
          "QPushButton:hover { background:#a00; }"
        : "QPushButton { background:#090; color:white; font-weight:bold; border-radius:4px; min-height:36px; }"
          "QPushButton:hover { background:#070; }");
    m_sendBtn->setEnabled(connected);
    m_sendFileBtn->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_parityCombo->setEnabled(!connected);
    m_dataBitsCombo->setEnabled(!connected);
    m_stopBitsCombo->setEnabled(!connected);
}

void SerialDebugWidget::onDataReceived(const QByteArray &data)
{
    if (m_autoClearCheck->isChecked() && m_receiveEdit->document()->characterCount() > 50000)
        m_receiveEdit->clear();

    appendToReceive(data);
}

void SerialDebugWidget::appendToReceive(const QByteArray &data)
{
    QString prefix;
    if (m_timestampCheck->isChecked()) {
        prefix = QString("<span style='color:#569cd6;'>time -&gt; %1</span><br>")
                     .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    }

    QString content;
    if (m_rxHexCheck->isChecked()) {
        content = QString("<span style='color:#9cdcfe;'>%1</span>").arg(HexUtils::toHexString(data).toHtmlEscaped());
    } else {
        content = QString("<span style='color:#d4d4d4;'>%1</span>").arg(QString::fromUtf8(data).toHtmlEscaped());
    }

    m_receiveEdit->moveCursor(QTextCursor::End);
    m_receiveEdit->insertHtml(prefix + content + "<br>");
    m_receiveEdit->verticalScrollBar()->setValue(m_receiveEdit->verticalScrollBar()->maximum());
}

QByteArray SerialDebugWidget::buildSendData() const
{
    QString text = m_sendEdit->toPlainText();
    if (m_txHexCheck->isChecked()) {
        bool ok;
        QByteArray data = HexUtils::fromHexString(text, &ok);
        if (!ok) return {};
        return data;
    }
    return text.toUtf8();
}

void SerialDebugWidget::onSend()
{
    QByteArray data = buildSendData();
    if (data.isEmpty()) return;
    m_serial->write(data);
    if (m_autoSendCheck->isChecked()) {
        m_autoSendCountSpin->setValue(m_autoSendCountSpin->value() + 1);
    }
}

void SerialDebugWidget::onClearReceive()
{
    m_receiveEdit->clear();
}

void SerialDebugWidget::onSaveReceive()
{
    QString path = QFileDialog::getSaveFileName(this, "保存接收数据", "", "文本文件 (*.txt);;所有文件 (*)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_receiveEdit->toPlainText().toUtf8());
        f.close();
    }
}

void SerialDebugWidget::onChooseFile()
{
    QString path = QFileDialog::getOpenFileName(this, "选择发送文件");
    if (!path.isEmpty()) {
        m_pendingFilePath = path;
        m_filePathEdit->setText(path);
    }
}

void SerialDebugWidget::onSendFile()
{
    if (m_pendingFilePath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择文件");
        return;
    }
    QFile f(m_pendingFilePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + f.errorString());
        return;
    }
    QByteArray data = f.readAll();
    f.close();
    m_serial->write(data);
}

void SerialDebugWidget::onAutoSendToggle(bool checked)
{
    if (checked) {
        m_autoSendCountSpin->setValue(0);
        m_autoSendTimer->start(m_autoSendIntervalSpin->value());
    } else {
        m_autoSendTimer->stop();
    }
}

void SerialDebugWidget::onCountClear()
{
    m_serial->resetStats();
    m_autoSendCountSpin->setValue(0);
    updateStatusBar();
}

void SerialDebugWidget::onRefreshPorts()
{
    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    for (const QString &p : SerialManager::availablePorts())
        m_portCombo->addItem(p);
    int idx = m_portCombo->findText(current);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
}

void SerialDebugWidget::updateStatusBar()
{
    m_rxLabel->setText(QString("RX: %1 bytes").arg(m_serial->rxBytes()));
    m_txLabel->setText(QString("TX: %1 bytes").arg(m_serial->txBytes()));
}
