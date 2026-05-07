#pragma once
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class SerialManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    // Port configuration
    bool open(const QString &portName,
              qint32 baudRate,
              QSerialPort::DataBits dataBits,
              QSerialPort::Parity parity,
              QSerialPort::StopBits stopBits);
    void close();
    bool isOpen() const;
    QString portName() const;

    // Data transfer
    qint64 write(const QByteArray &data);

    // Statistics
    quint64 rxBytes() const { return m_rxBytes; }
    quint64 txBytes() const { return m_txBytes; }
    void resetStats();

    // Available ports
    static QStringList availablePorts();

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &errorString);
    void portOpened();
    void portClosed();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_port;
    quint64 m_rxBytes = 0;
    quint64 m_txBytes = 0;
};
