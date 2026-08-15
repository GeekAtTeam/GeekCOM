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
    void onThemeChanged();

private:
    void setupUi();
    void setupConnections();
    void applyThemeStyles();
    void processVT100(const QByteArray &data);
    void sendBytes(const QByteArray &data);

    bool eventFilter(QObject *obj, QEvent *event) override;

    SerialManager *m_serial;
    QTextEdit *m_terminal;
    SerialPortConfigGroup *m_portConfig = nullptr;

    QCheckBox *m_localEchoCheck;
    QCheckBox *m_crlfCheck;
    QPushButton *m_clearBtn;
    QLabel *m_statusLabel;

    QString m_escBuffer;
    bool m_inEscape = false;
    bool m_connected = false;
};
