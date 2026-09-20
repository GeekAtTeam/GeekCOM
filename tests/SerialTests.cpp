#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include "HexUtils.h"
#include "MainWindow.h"
#include "SerialDebugWidget.h"
#include "SerialTerminalWidget.h"
#include "SerialPortConfigGroup.h"
#include "SerialManager.h"

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

struct Pty {
    int fd = -1;
    QString path;
    Pty() {
        fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0 && grantpt(fd) == 0 && unlockpt(fd) == 0)
            path = QString::fromLocal8Bit(ptsname(fd));
    }
    ~Pty() { if (fd >= 0) ::close(fd); }
    QByteArray drain() {
        QByteArray data;
        char buffer[4096];
        ssize_t count;
        while ((count = ::read(fd, buffer, sizeof(buffer))) > 0)
            data.append(buffer, count);
        return data;
    }
};
#endif

class SerialTests : public QObject {
    Q_OBJECT
private slots:
    void hex_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("continuous") << QString("0123aB") << true << QByteArray::fromHex("0123ab");
        QTest::newRow("whitespace") << QString(" 01\t23\nFF ") << true << QByteArray::fromHex("0123ff");
        QTest::newRow("empty") << QString(" ") << true << QByteArray();
        for (const auto &value : {"123", "1 23", "GG", "0x01", "+1", "01 2345", "01,23"})
            QTest::newRow(value) << QString(value) << false << QByteArray();
    }
    void hex() {
        QFETCH(QString, input);
        QFETCH(bool, valid);
        QFETCH(QByteArray, bytes);
        bool ok = false;
        QString error;
        QCOMPARE(HexUtils::fromHexString(input, &ok, &error), bytes);
        QCOMPARE(ok, valid);
        QCOMPARE(error.isEmpty(), valid);
    }
    void failedConnectionAndWrite() {
        SerialManager serial;
        QSignalSpy errors(&serial, &SerialManager::errorOccurred);
        QVERIFY(!serial.open("/nonexistent/geekcom-test", 115200, QSerialPort::Data8,
                             QSerialPort::NoParity, QSerialPort::OneStop));
        QVERIFY(!serial.lastError().isEmpty());
        QVERIFY(!errors.isEmpty());
        QCOMPARE(serial.write("AT"), qint64(-1));
        QCOMPARE(serial.txBytes(), quint64(0));
    }
    void serialAndUi() {
#ifndef Q_OS_LINUX
        QSKIP("PTY integration test requires Linux");
#else
        Pty peer;
        QVERIFY(!peer.path.isEmpty());
        MainWindow window;
        auto *serial = window.findChild<SerialManager *>();
        auto *debug = window.findChild<SerialDebugWidget *>();
        auto *terminal = window.findChild<SerialTerminalWidget *>();
        QVERIFY(serial && debug && terminal);
        auto *debugConfig = debug->findChild<SerialPortConfigGroup *>();
        auto *termConfig = terminal->findChild<SerialPortConfigGroup *>();
        auto *editor = debug->findChild<QTextEdit *>("sendEditor");
        auto *hex = debug->findChild<QCheckBox *>("sendHex");
        auto *periodic = debug->findChild<QCheckBox *>("autoSend");
        auto *ending = debug->findChild<QComboBox *>("lineEnding");
        QVERIFY(editor && hex && periodic && ending);
        QVERIFY(!periodic->isEnabled());
        debugConfig->portCombo()->addItem("/nonexistent/geekcom-test");
        debugConfig->portCombo()->setCurrentText("/nonexistent/geekcom-test");
        debugConfig->connectButton()->click();
        QVERIFY(!debugConfig->connectButton()->isChecked());
        QVERIFY(debugConfig->portCombo()->isEnabled());
        QVERIFY(!serial->isOpen());
        QVERIFY(serial->open(peer.path, 115200, QSerialPort::Data8,
                             QSerialPort::NoParity, QSerialPort::OneStop));
        QVERIFY(debugConfig->connectButton()->isChecked());
        QVERIFY(termConfig->connectButton()->isChecked());
        QVERIFY(!debugConfig->portCombo()->isEnabled());
        QByteArray actual;
        for (int i = 0; i < ending->count(); ++i) {
            editor->setPlainText(QString::fromUtf8("中文"));
            ending->setCurrentIndex(i);
            const QByteArray expected = QString::fromUtf8("中文").toUtf8() + ending->itemData(i).toByteArray();
            QVERIFY(debug->findChild<QLabel *>("sendPreview")->text().contains(HexUtils::toHexString(expected)));
            QVERIFY(QMetaObject::invokeMethod(debug, "onSend"));
            actual.clear();
            QTRY_VERIFY((actual += peer.drain()).size() >= expected.size());
            QCOMPARE(actual, expected);
        }
        hex->setChecked(true);
        editor->setPlainText("00 FF 0D 0A");
        QVERIFY(QMetaObject::invokeMethod(debug, "onSend"));
        actual.clear();
        QTRY_VERIFY((actual += peer.drain()).size() >= 4);
        QCOMPARE(actual, QByteArray::fromHex("00ff0d0a"));
        const auto submitted = serial->txBytes();
        editor->setPlainText("1 23");
        QVERIFY(QMetaObject::invokeMethod(debug, "onSend"));
        QCOMPARE(serial->txBytes(), submitted);
        QVERIFY(peer.drain().isEmpty());

        QSignalSpy received(serial, &SerialManager::dataReceived);
        const QByteArray incoming = QByteArray::fromHex("00ff0d0a") + QString::fromUtf8("中文").toUtf8();
        QCOMPARE(::write(peer.fd, incoming.constData(), incoming.size()), ssize_t(incoming.size()));
        QTRY_VERIFY(!received.isEmpty());
        QByteArray rx;
        for (const auto &event : received) rx += event.at(0).toByteArray();
        QCOMPARE(rx, incoming);

        editor->setPlainText("01 23");
        periodic->setChecked(true);
        QVERIFY(editor->isReadOnly());
        // Exercise the same ResourceError path used on device removal.
        QVERIFY(QMetaObject::invokeMethod(serial, "onErrorOccurred", Qt::DirectConnection,
            Q_ARG(QSerialPort::SerialPortError, QSerialPort::ResourceError)));
        QVERIFY(!serial->isOpen());
        QVERIFY(!periodic->isChecked());
        QVERIFY(!editor->isReadOnly());
        QVERIFY(!debugConfig->connectButton()->isChecked());
        QVERIFY(!termConfig->connectButton()->isChecked());
        QVERIFY(debugConfig->portCombo()->isEnabled());
        bool errorVisible = false;
        for (auto *label : window.findChildren<QLabel *>())
            errorVisible |= label->text().startsWith("已断开 · 错误:");
        QVERIFY(errorVisible);
        QVERIFY(serial->open(peer.path, 115200, QSerialPort::Data8,
                             QSerialPort::NoParity, QSerialPort::OneStop));
        QVERIFY(!periodic->isChecked());
        QTest::qWait(1100);
        QVERIFY(peer.drain().isEmpty());
        QCOMPARE(serial->txBytes(), quint64(0));
        serial->close();
        QVERIFY(!termConfig->connectButton()->isChecked());
        QVERIFY(QMetaObject::invokeMethod(debug, "onSend"));
        QCOMPARE(serial->txBytes(), quint64(0));
#endif
    }
};

QTEST_MAIN(SerialTests)
#include "SerialTests.moc"
