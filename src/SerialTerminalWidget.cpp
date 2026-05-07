#include "SerialTerminalWidget.h"
#include "SerialManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QScrollBar>
#include <QKeyEvent>
#include <QEvent>
#include <QSerialPort>
#include <QFrame>
#include <QMessageBox>

SerialTerminalWidget::SerialTerminalWidget(SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_serial(serial)
{
    setupUi();
    setupConnections();
    applyConnectedState(false);
}

void SerialTerminalWidget::setupUi()
{
    // ===================== Right Config Panel =====================
    auto *rightPanel = new QWidget;
    rightPanel->setFixedWidth(280);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    auto *portGroup = new QGroupBox("串口配置");
    auto *portGrid = new QGridLayout(portGroup);
    portGrid->setSpacing(6);

    portGrid->addWidget(new QLabel("端口"), 0, 0);
    m_portCombo = new QComboBox;
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
    m_parityCombo->addItem("None", QSerialPort::NoParity);
    m_parityCombo->addItem("Odd",  QSerialPort::OddParity);
    m_parityCombo->addItem("Even", QSerialPort::EvenParity);
    portGrid->addWidget(m_parityCombo, 2, 1);

    portGrid->addWidget(new QLabel("数据位"), 3, 0);
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem("8", QSerialPort::Data8);
    m_dataBitsCombo->addItem("7", QSerialPort::Data7);
    portGrid->addWidget(m_dataBitsCombo, 3, 1);

    portGrid->addWidget(new QLabel("停止位"), 4, 0);
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem("1", QSerialPort::OneStop);
    m_stopBitsCombo->addItem("2", QSerialPort::TwoStop);
    portGrid->addWidget(m_stopBitsCombo, 4, 1);

    m_connectBtn = new QPushButton("打开串口");
    m_connectBtn->setCheckable(true);
    m_connectBtn->setMinimumHeight(36);
    portGrid->addWidget(m_connectBtn, 5, 0, 1, 2);

    rightLayout->addWidget(portGroup);

    // Terminal options
    auto *termGroup = new QGroupBox("终端设置");
    auto *termLayout = new QVBoxLayout(termGroup);
    m_localEchoCheck = new QCheckBox("本地回显");
    m_crlfCheck = new QCheckBox("回车发送 \\r\\n");
    m_crlfCheck->setChecked(true);
    m_clearBtn = new QPushButton("清屏");
    termLayout->addWidget(m_localEchoCheck);
    termLayout->addWidget(m_crlfCheck);
    termLayout->addWidget(m_clearBtn);
    rightLayout->addWidget(termGroup);

    rightLayout->addStretch();

    m_statusLabel = new QLabel("未连接");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #888;");
    rightLayout->addWidget(m_statusLabel);

    // ===================== Terminal Area =====================
    m_terminal = new QTextEdit;
    m_terminal->setReadOnly(false); // We handle input manually
    m_terminal->setFont(QFont("Courier New", 10));
    m_terminal->setStyleSheet("background:#0c0c0c; color:#cccccc; border:none;");
    m_terminal->installEventFilter(this);
    // Prevent normal text input (we handle it ourselves)
    m_terminal->setUndoRedoEnabled(false);

    // ===================== Root =====================
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);
    rootLayout->addWidget(m_terminal, 1);
    rootLayout->addWidget(rightPanel, 0);

    onRefreshPorts();
}

void SerialTerminalWidget::setupConnections()
{
    connect(m_connectBtn,    &QPushButton::toggled, this, &SerialTerminalWidget::onToggleConnection);
    connect(m_clearBtn,      &QPushButton::clicked, this, &SerialTerminalWidget::onClearScreen);
    connect(m_refreshPortBtn,&QPushButton::clicked, this, &SerialTerminalWidget::onRefreshPorts);
}

void SerialTerminalWidget::onToggleConnection()
{
    if (m_connectBtn->isChecked()) {
        QString port = m_portCombo->currentText();
        int baud = m_baudCombo->currentData().toInt();
        auto parity   = (QSerialPort::Parity)  m_parityCombo->currentData().toInt();
        auto dataBits = (QSerialPort::DataBits) m_dataBitsCombo->currentData().toInt();
        auto stopBits = (QSerialPort::StopBits) m_stopBitsCombo->currentData().toInt();

        if (!m_serial->open(port, baud, dataBits, parity, stopBits)) {
            m_connectBtn->setChecked(false);
            QMessageBox::warning(this, "连接失败", "无法打开串口，请检查端口设置。");
            return;
        }
        applyConnectedState(true);
        m_terminal->setFocus();
    } else {
        m_serial->close();
        applyConnectedState(false);
    }
}

void SerialTerminalWidget::applyConnectedState(bool connected)
{
    m_connectBtn->setText(connected ? "关闭串口" : "打开串口");
    m_connectBtn->setStyleSheet(connected
        ? "QPushButton { background:#c00; color:white; font-weight:bold; border-radius:4px; min-height:36px; }"
          "QPushButton:hover { background:#a00; }"
        : "QPushButton { background:#090; color:white; font-weight:bold; border-radius:4px; min-height:36px; }"
          "QPushButton:hover { background:#070; }");
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_parityCombo->setEnabled(!connected);
    m_dataBitsCombo->setEnabled(!connected);
    m_stopBitsCombo->setEnabled(!connected);
    m_statusLabel->setText(connected
        ? QString("已连接 %1").arg(m_serial->portName())
        : "未连接");
    m_statusLabel->setStyleSheet(connected ? "color:#0c0;" : "color:#888;");
}

void SerialTerminalWidget::onDataReceived(const QByteArray &data)
{
    processVT100(data);
}

void SerialTerminalWidget::processVT100(const QByteArray &data)
{
    // Basic VT100: handle \r, \n, \b and strip escape sequences
    QString text;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = (unsigned char)data[i];
        if (m_inEscape) {
            m_escBuffer += (char)c;
            // End of escape sequence
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~') {
                // For now: discard escape sequences (full VT100 is complex)
                m_inEscape = false;
                m_escBuffer.clear();
            }
            continue;
        }
        if (c == 0x1B) { // ESC
            m_inEscape = true;
            m_escBuffer = "\x1B";
            continue;
        }
        if (c == '\r') continue; // ignore CR (we use \n for newline)
        if (c == '\n') { text += '\n'; continue; }
        if (c == '\b') {
            // Backspace
            QTextCursor cursor = m_terminal->textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            continue;
        }
        if (c >= 0x20 || c == '\t') {
            text += QChar(c);
        }
    }
    if (!text.isEmpty()) {
        QTextCursor cursor = m_terminal->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_terminal->setTextCursor(cursor);
        m_terminal->insertPlainText(text);
        m_terminal->verticalScrollBar()->setValue(m_terminal->verticalScrollBar()->maximum());
    }
}

void SerialTerminalWidget::sendBytes(const QByteArray &data)
{
    if (!m_serial->isOpen()) return;
    m_serial->write(data);
    if (m_localEchoCheck->isChecked()) {
        processVT100(data);
    }
}

bool SerialTerminalWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_terminal && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (!m_serial->isOpen()) return true; // consume but ignore

        QByteArray toSend;
        int key = keyEvent->key();
        QString text = keyEvent->text();

        switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            toSend = m_crlfCheck->isChecked() ? QByteArray("\r\n") : QByteArray("\r");
            break;
        case Qt::Key_Backspace:
            toSend = QByteArray("\x08");
            break;
        case Qt::Key_Tab:
            toSend = QByteArray("\t");
            break;
        case Qt::Key_Up:
            toSend = QByteArray("\x1B[A");
            break;
        case Qt::Key_Down:
            toSend = QByteArray("\x1B[B");
            break;
        case Qt::Key_Right:
            toSend = QByteArray("\x1B[C");
            break;
        case Qt::Key_Left:
            toSend = QByteArray("\x1B[D");
            break;
        case Qt::Key_Delete:
            toSend = QByteArray("\x1B[3~");
            break;
        case Qt::Key_Home:
            toSend = QByteArray("\x1B[H");
            break;
        case Qt::Key_End:
            toSend = QByteArray("\x1B[F");
            break;
        default:
            if (!text.isEmpty()) {
                toSend = text.toUtf8();
            }
            break;
        }

        if (!toSend.isEmpty()) {
            sendBytes(toSend);
        }
        return true; // Consume all key events
    }
    return QWidget::eventFilter(obj, event);
}

void SerialTerminalWidget::onClearScreen()
{
    m_terminal->clear();
}

void SerialTerminalWidget::onRefreshPorts()
{
    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    for (const QString &p : SerialManager::availablePorts())
        m_portCombo->addItem(p);
    int idx = m_portCombo->findText(current);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
}
