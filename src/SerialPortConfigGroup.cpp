#include "SerialPortConfigGroup.h"
#include "SerialBaudRates.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSerialPort>
#include <QSizePolicy>

SerialPortConfigGroup::SerialPortConfigGroup(QWidget *parent)
    : QGroupBox(QStringLiteral("串口配置"), parent)
{
    auto *portGrid = new QGridLayout(this);
    portGrid->setSpacing(6);

    portGrid->addWidget(new QLabel(QStringLiteral("端口")), 0, 0);
    m_portCombo = new QComboBox;
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_refreshPortBtn = new QPushButton(QStringLiteral("↻"));
    m_refreshPortBtn->setFixedWidth(28);
    m_refreshPortBtn->setToolTip(QStringLiteral("刷新串口列表"));
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_portCombo);
    portRow->addWidget(m_refreshPortBtn);
    portGrid->addLayout(portRow, 0, 1);

    portGrid->addWidget(new QLabel(QStringLiteral("波特率")), 1, 0);
    m_baudCombo = new QComboBox;
    SerialBaudRates::populateBaudCombo(m_baudCombo);
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    portGrid->addWidget(m_baudCombo, 1, 1);

    portGrid->addWidget(new QLabel(QStringLiteral("校验位")), 2, 0);
    m_parityCombo = new QComboBox;
    m_parityCombo->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    m_parityCombo->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);
    m_parityCombo->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    portGrid->addWidget(m_parityCombo, 2, 1);

    portGrid->addWidget(new QLabel(QStringLiteral("数据位")), 3, 0);
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem(QStringLiteral("5"), QSerialPort::Data5);
    m_dataBitsCombo->addItem(QStringLiteral("6"), QSerialPort::Data6);
    m_dataBitsCombo->addItem(QStringLiteral("7"), QSerialPort::Data7);
    m_dataBitsCombo->addItem(QStringLiteral("8"), QSerialPort::Data8);
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    portGrid->addWidget(m_dataBitsCombo, 3, 1);

    portGrid->addWidget(new QLabel(QStringLiteral("停止位")), 4, 0);
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    m_stopBitsCombo->addItem(QStringLiteral("1.5"), QSerialPort::OneAndHalfStop);
    m_stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
    portGrid->addWidget(m_stopBitsCombo, 4, 1);

    m_connectBtn = new QPushButton(QStringLiteral("打开串口"));
    m_connectBtn->setCheckable(true);
    m_connectBtn->setMinimumHeight(36);
    portGrid->addWidget(m_connectBtn, 5, 0, 1, 2);
}

void SerialPortConfigGroup::setParameterFieldsEnabled(bool enabled)
{
    m_portCombo->setEnabled(enabled);
    m_refreshPortBtn->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_parityCombo->setEnabled(enabled);
    m_dataBitsCombo->setEnabled(enabled);
    m_stopBitsCombo->setEnabled(enabled);
}
