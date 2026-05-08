#pragma once
#include <QWidget>
#include <QByteArray>

class SerialManager;
class SerialPortConfigGroup;
class QTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QKeyEvent;

/**
 * 串口终端模式
 * - VT100 基础终端仿真
 * - 键盘输入直接通过串口发送
 * - 支持本地回显
 */
class SerialTerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SerialTerminalWidget(SerialManager *serial, QWidget *parent = nullptr);

    void onDataReceived(const QByteArray &data);

private slots:
    void onToggleConnection();
    void onClearScreen();
    void onRefreshPorts();
    void applyConnectedState(bool connected);

private:
    void setupUi();
    void setupConnections();
    void processVT100(const QByteArray &data);
    void sendBytes(const QByteArray &data);

    // Override key events via event filter on terminal widget
    bool eventFilter(QObject *obj, QEvent *event) override;

    SerialManager *m_serial;

    // Left: terminal display
    QTextEdit *m_terminal;

    SerialPortConfigGroup *m_portConfig = nullptr;

    // Terminal options
    QCheckBox *m_localEchoCheck;
    QCheckBox *m_crlfCheck;       // append \r\n on Enter
    QPushButton *m_clearBtn;

    // Status
    QLabel *m_statusLabel;

    // VT100 escape sequence buffer
    QString m_escBuffer;
    bool m_inEscape = false;
};
