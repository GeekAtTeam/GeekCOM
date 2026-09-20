#include "SerialManager.h"
#include <QDebug>

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialManager::onErrorOccurred);
}

SerialManager::~SerialManager()
{
    close();
}

bool SerialManager::open(const QString &portName,
                         qint32 baudRate,
                         QSerialPort::DataBits dataBits,
                         QSerialPort::Parity parity,
                         QSerialPort::StopBits stopBits)
{
    close();
    m_lastError.clear();

    m_port->setPortName(portName);
    if (!m_port->setBaudRate(baudRate) || !m_port->setDataBits(dataBits)
        || !m_port->setParity(parity) || !m_port->setStopBits(stopBits)
        || !m_port->setFlowControl(QSerialPort::NoFlowControl)) {
        m_lastError = m_port->errorString();
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_port->open(QIODevice::ReadWrite)) {
        m_lastError = m_port->errorString();
        emit errorOccurred(m_lastError);
        return false;
    }
    resetStats();
    emit portOpened();
    return true;
}

void SerialManager::close()
{
    if (m_port->isOpen()) {
        m_port->close();
        emit portClosed();
    }
}

bool SerialManager::isOpen() const
{
    return m_port->isOpen();
}

QString SerialManager::portName() const
{
    return m_port->portName();
}

qint64 SerialManager::write(const QByteArray &data)
{
    if (!m_port->isOpen()) {
        m_lastError = tr("串口未连接，数据未提交。");
        emit errorOccurred(m_lastError);
        return -1;
    }
    qint64 written = m_port->write(data);
    if (written > 0) m_txBytes += written;
    if (written != data.size()) {
        m_lastError = written < 0 ? m_port->errorString()
            : tr("仅提交 %1 / %2 字节；请检查连接，不会自动重发。").arg(written).arg(data.size());
        emit errorOccurred(m_lastError);
    }
    return written;
}

void SerialManager::resetStats()
{
    m_rxBytes = 0;
    m_txBytes = 0;
}

QStringList SerialManager::availablePorts()
{
    QStringList ports;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    return ports;
}

void SerialManager::onReadyRead()
{
    QByteArray data = m_port->readAll();
    if (!data.isEmpty()) {
        m_rxBytes += data.size();
        emit dataReceived(data);
    }
}

void SerialManager::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        m_lastError = m_port->errorString();
        emit errorOccurred(m_lastError);
        if (error == QSerialPort::ResourceError) {
            close();
        }
    }
}
