#include "SerialDebugWidget.h"
#include "SerialManager.h"
#include "SerialPortConfigGroup.h"
#include "HexUtils.h"
#include "ThemeManager.h"

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
#include <QSignalBlocker>

SerialDebugWidget::SerialDebugWidget(SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_serial(serial)
    , m_autoSendTimer(new QTimer(this))
{
    setupUi();
    setupConnections();
    applyConnectedState(m_serial->isOpen());
    applyThemeStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &SerialDebugWidget::onThemeChanged);
}

void SerialDebugWidget::setupUi()
{
    // ===================== Right Config Panel =====================
    auto *rightPanel = new QWidget;
    rightPanel->setFixedWidth(280);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    m_portConfig = new SerialPortConfigGroup;
    rightLayout->addWidget(m_portConfig);

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
    m_txHexCheck->setObjectName("sendHex");
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
    m_autoSendCheck->setObjectName("autoSend");
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
    m_rxTitle = new QLabel("接收区");
    m_receiveEdit = new QTextEdit;
    m_receiveEdit->setReadOnly(true);
    m_receiveEdit->setFont(QFont("Courier New", 10));
    m_receiveEdit->setProperty("logView", true);
    rxVBox->addWidget(m_rxTitle);
    rxVBox->addWidget(m_receiveEdit);
    splitter->addWidget(rxWidget);

    // Send area
    auto *txWidget = new QWidget;
    auto *txVBox = new QVBoxLayout(txWidget);
    txVBox->setContentsMargins(4, 4, 4, 4);
    txVBox->setSpacing(4);
    m_txTitle = new QLabel("发送区");
    m_sendEdit = new QTextEdit;
    m_sendEdit->setObjectName("sendEditor");
    m_sendEdit->setFont(QFont("Courier New", 10));
    m_sendEdit->setPlaceholderText("输入要发送的数据...");
    m_sendEdit->setMaximumHeight(120);
    m_sendBtn = new QPushButton("发 送");
    m_sendBtn->setFixedHeight(36);
    txVBox->addWidget(m_txTitle);
    txVBox->addWidget(m_sendEdit);
    m_lineEndingCombo = new QComboBox;
    m_lineEndingCombo->setObjectName("lineEnding");
    m_lineEndingCombo->addItem("无行尾", QByteArray());
    m_lineEndingCombo->addItem("LF", QByteArray("\n"));
    m_lineEndingCombo->addItem("CR", QByteArray("\r"));
    m_lineEndingCombo->addItem("CRLF", QByteArray("\r\n"));
    m_sendPreview = new QLabel;
    m_sendPreview->setObjectName("sendPreview");
    m_sendPreview->setWordWrap(true);
    m_sendResult = new QLabel;
    m_sendResult->setWordWrap(true);
    txVBox->addWidget(m_lineEndingCombo);
    txVBox->addWidget(m_sendPreview);
    txVBox->addWidget(m_sendResult);
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

    m_autoSendLabel = new QLabel("周期已提交次数:");
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
    connect(m_portConfig->connectButton(), &QPushButton::toggled, this, &SerialDebugWidget::onToggleConnection);
    connect(m_sendBtn,       &QPushButton::clicked, this, &SerialDebugWidget::onSend);
    connect(m_clearRxBtn,    &QPushButton::clicked, this, &SerialDebugWidget::onClearReceive);
    connect(m_saveRxBtn,     &QPushButton::clicked, this, &SerialDebugWidget::onSaveReceive);
    connect(m_chooseFileBtn, &QPushButton::clicked, this, &SerialDebugWidget::onChooseFile);
    connect(m_sendFileBtn,   &QPushButton::clicked, this, &SerialDebugWidget::onSendFile);
    connect(m_countClearBtn, &QPushButton::clicked, this, &SerialDebugWidget::onCountClear);
    connect(m_portConfig->refreshPortButton(), &QPushButton::clicked, this, &SerialDebugWidget::onRefreshPorts);
    connect(m_autoSendCheck, &QCheckBox::toggled,   this, &SerialDebugWidget::onAutoSendToggle);

    connect(m_autoSendTimer, &QTimer::timeout, this, &SerialDebugWidget::onSend);

    connect(m_serial, &SerialManager::portOpened, this, [this] { applyConnectedState(true); });
    connect(m_serial, &SerialManager::portClosed, this, [this] { applyConnectedState(false); });
    connect(m_serial, &SerialManager::errorOccurred, this, [this](const QString &error) {
        m_autoSendCheck->setChecked(false);
        m_sendResult->setText("错误: " + error);
    });
    connect(m_sendEdit, &QTextEdit::textChanged, this, &SerialDebugWidget::updateSendPreview);
    connect(m_txHexCheck, &QCheckBox::toggled, this, &SerialDebugWidget::updateSendPreview);
    connect(m_lineEndingCombo, &QComboBox::currentIndexChanged, this, &SerialDebugWidget::updateSendPreview);

    // Update status bar periodically
    auto *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &SerialDebugWidget::updateStatusBar);
    statusTimer->start(500);
}

void SerialDebugWidget::onToggleConnection()
{
    if (m_portConfig->connectButton()->isChecked()) {
        QString port = m_portConfig->portCombo()->currentText();
        int baud = m_portConfig->baudCombo()->currentData().toInt();
        auto parity   = (QSerialPort::Parity)  m_portConfig->parityCombo()->currentData().toInt();
        auto dataBits = (QSerialPort::DataBits) m_portConfig->dataBitsCombo()->currentData().toInt();
        auto stopBits = (QSerialPort::StopBits) m_portConfig->stopBitsCombo()->currentData().toInt();

        if (!m_serial->open(port, baud, dataBits, parity, stopBits)) {
            applyConnectedState(false);
            m_sendResult->setText("连接失败: " + m_serial->lastError());
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
    m_connected = connected;
    const QSignalBlocker blocker(m_portConfig->connectButton());
    m_portConfig->connectButton()->setChecked(connected);
    if (!connected) {
        m_autoSendTimer->stop();
        m_autoSendCheck->setChecked(false);
    }
    m_portConfig->connectButton()->setText(connected ? "关闭串口" : "打开串口");
    updateSendPreview();
    m_sendFileBtn->setEnabled(connected);
    m_portConfig->setParameterFieldsEnabled(!connected);
    applyThemeStyles();
}

void SerialDebugWidget::onThemeChanged()
{
    applyThemeStyles();
}

void SerialDebugWidget::applyThemeStyles()
{
    auto &t = ThemeManager::instance();
    if (m_rxTitle)
        m_rxTitle->setStyleSheet(t.styleMutedText(true));
    if (m_txTitle)
        m_txTitle->setStyleSheet(t.styleMutedText(true));
    m_receiveEdit->setStyleSheet(t.styleLogView(true));
    m_sendBtn->setStyleSheet(t.stylePrimaryButton(36));
    m_portConfig->connectButton()->setStyleSheet(
        m_connected ? t.styleDangerButton(36) : t.styleSuccessButton(36));
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
    auto &t = ThemeManager::instance();
    if (m_timestampCheck->isChecked()) {
        prefix = QString("<span style='color:%1;'>time -&gt; %2</span><br>")
                     .arg(t.css(t.colors().logTimestamp),
                          QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    }

    QString content;
    if (m_rxHexCheck->isChecked()) {
        content = QString("<span style='color:%1;'>%2</span>")
                      .arg(t.css(t.colors().logHex), HexUtils::toHexString(data).toHtmlEscaped());
    } else {
        content = QString("<span style='color:%1;'>%2</span>")
                      .arg(t.css(t.colors().logFg), QString::fromUtf8(data).toHtmlEscaped());
    }

    m_receiveEdit->moveCursor(QTextCursor::End);
    m_receiveEdit->insertHtml(prefix + content + "<br>");
    m_receiveEdit->verticalScrollBar()->setValue(m_receiveEdit->verticalScrollBar()->maximum());
}

QByteArray SerialDebugWidget::buildSendData(QString *error) const
{
    QString text = m_sendEdit->toPlainText();
    if (m_txHexCheck->isChecked()) {
        bool ok;
        QByteArray data = HexUtils::fromHexString(text, &ok, error);
        if (!ok) return {};
        return data;
    }
    return text.toUtf8() + m_lineEndingCombo->currentData().toByteArray();
}

void SerialDebugWidget::updateSendPreview()
{
    QString error;
    const QByteArray data = buildSendData(&error);
    m_lineEndingCombo->setEnabled(!m_txHexCheck->isChecked() && !m_autoSendTimer->isActive());
    m_sendPreview->setText(error.isEmpty()
        ? QString("%1 字节 · %2%3").arg(data.size()).arg(HexUtils::toHexString(data.left(64)),
            data.size() > 64 ? " …（预览前 64 字节）" : "") : error);
    const bool canSend = m_serial->isOpen() && error.isEmpty() && !data.isEmpty();
    m_sendBtn->setEnabled(canSend && !m_autoSendTimer->isActive());
    m_autoSendCheck->setEnabled(canSend || m_autoSendTimer->isActive());
}

void SerialDebugWidget::onSend()
{
    QString error;
    const QByteArray data = buildSendData(&error);
    if (!error.isEmpty() || data.isEmpty()) {
        m_autoSendCheck->setChecked(false);
        m_sendResult->setText(error.isEmpty() ? "没有可发送的数据" : error);
        return;
    }
    const qint64 accepted = m_serial->write(data);
    if (accepted != data.size()) {
        m_autoSendCheck->setChecked(false);
        m_sendResult->setText("发送失败: " + m_serial->lastError());
        return;
    }
    m_sendResult->setText(QString("已提交 %1 字节（不代表设备已收到）").arg(accepted));
    if (m_autoSendCheck->isChecked())
        m_autoSendCountSpin->setValue(m_autoSendCountSpin->value() + 1);
    updateStatusBar();
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
    const qint64 accepted = m_serial->write(data);
    m_sendResult->setText(accepted == data.size()
        ? QString("文件已提交 %1 字节（不代表设备已收到）").arg(accepted)
        : "文件发送失败: " + m_serial->lastError());
}

void SerialDebugWidget::onAutoSendToggle(bool checked)
{
    if (checked && (!m_serial->isOpen() || buildSendData().isEmpty())) {
        m_autoSendCheck->setChecked(false);
        return;
    }
    if (checked) {
        m_autoSendCountSpin->setValue(0);
        m_autoSendTimer->start(m_autoSendIntervalSpin->value());
    } else {
        m_autoSendTimer->stop();
    }
    m_sendEdit->setReadOnly(checked);
    m_txHexCheck->setEnabled(!checked);
    m_autoSendIntervalSpin->setEnabled(!checked);
    m_sendFileBtn->setEnabled(m_serial->isOpen() && !checked);
    updateSendPreview();
}

void SerialDebugWidget::onCountClear()
{
    m_serial->resetStats();
    m_autoSendCountSpin->setValue(0);
    updateStatusBar();
}

void SerialDebugWidget::onRefreshPorts()
{
    QString current = m_portConfig->portCombo()->currentText();
    m_portConfig->portCombo()->clear();
    for (const QString &p : SerialManager::availablePorts())
        m_portConfig->portCombo()->addItem(p);
    int idx = m_portConfig->portCombo()->findText(current);
    if (idx >= 0) m_portConfig->portCombo()->setCurrentIndex(idx);
}

void SerialDebugWidget::updateStatusBar()
{
    m_rxLabel->setText(QString("RX: %1 bytes").arg(m_serial->rxBytes()));
    m_txLabel->setText(QString("TX 已提交: %1 bytes").arg(m_serial->txBytes()));
}
