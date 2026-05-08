#pragma once

#include <QGroupBox>

class QComboBox;
class QPushButton;

/**
 * 右侧「串口配置」分组：端口、波特率、校验、数据位、停止位、打开/关闭串口。
 * 供串口调试与串口终端界面复用。
 */
class SerialPortConfigGroup : public QGroupBox
{
    Q_OBJECT
public:
    explicit SerialPortConfigGroup(QWidget *parent = nullptr);

    QComboBox *portCombo() const { return m_portCombo; }
    QPushButton *refreshPortButton() const { return m_refreshPortBtn; }
    QComboBox *baudCombo() const { return m_baudCombo; }
    QComboBox *parityCombo() const { return m_parityCombo; }
    QComboBox *dataBitsCombo() const { return m_dataBitsCombo; }
    QComboBox *stopBitsCombo() const { return m_stopBitsCombo; }
    QPushButton *connectButton() const { return m_connectBtn; }

    /** 连接串口后为 false，用于禁用端口与参数控件（连接按钮本身仍可用）。 */
    void setParameterFieldsEnabled(bool enabled);

private:
    QComboBox *m_portCombo = nullptr;
    QPushButton *m_refreshPortBtn = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
};
