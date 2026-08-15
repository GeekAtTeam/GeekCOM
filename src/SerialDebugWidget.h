#pragma once
#include <QWidget>
#include <QByteArray>
#include <QTimer>

class SerialManager;
class SerialPortConfigGroup;
class QTextEdit;
class QComboBox;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QLabel;
class QSpinBox;
class QGroupBox;

/**
 * 串口调试模式
 * - 左侧：接收区（上）+ 发送区（下）
 * - 右侧：串口配置 + 接收选项 + 发送选项
 */
class SerialDebugWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SerialDebugWidget(SerialManager *serial, QWidget *parent = nullptr);

    void onDataReceived(const QByteArray &data);

private slots:
    void onToggleConnection();
    void onSend();
    void onClearReceive();
    void onSaveReceive();
    void onChooseFile();
    void onSendFile();
    void onAutoSendToggle(bool checked);
    void onCountClear();
    void onRefreshPorts();
    void updateStatusBar();
    void onThemeChanged();

private:
    void setupUi();
    void setupConnections();
    void appendToReceive(const QByteArray &data);
    void applyConnectedState(bool connected);
    void applyThemeStyles();
    QByteArray buildSendData() const;

    SerialManager *m_serial;
    SerialPortConfigGroup *m_portConfig = nullptr;

    QCheckBox *m_rxHexCheck;
    QCheckBox *m_timestampCheck;
    QCheckBox *m_autoClearCheck;
    QPushButton *m_clearRxBtn;
    QPushButton *m_saveRxBtn;

    QLabel *m_rxTitle = nullptr;
    QTextEdit *m_receiveEdit;

    QLabel *m_txTitle = nullptr;
    QTextEdit *m_sendEdit;

    QCheckBox *m_txHexCheck;
    QLineEdit *m_filePathEdit;
    QPushButton *m_chooseFileBtn;
    QPushButton *m_sendFileBtn;
    QCheckBox *m_autoSendCheck;
    QSpinBox *m_autoSendIntervalSpin;
    QPushButton *m_sendBtn;

    QLabel *m_rxLabel;
    QLabel *m_txLabel;
    QPushButton *m_countClearBtn;
    QLabel *m_autoSendLabel;
    QSpinBox *m_autoSendCountSpin;

    QTimer *m_autoSendTimer;
    QString m_pendingFilePath;
    QByteArray m_rxBuffer;
    bool m_connected = false;
};
