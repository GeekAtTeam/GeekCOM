// Linux-only benchmark adapter: uses the real application widgets and serial path.
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTextCursor>
#include <iostream>
#include "MainWindow.h"
#include "SerialManager.h"
#include "SerialDebugWidget.h"
#include "SerialTerminalWidget.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc != 4) return 2; // port, debug|terminal, seconds
    MainWindow window;
    window.resize(1280, 820);
    const bool terminal = QString::fromLocal8Bit(argv[2]) == "terminal";
    auto *tabs = window.findChild<QTabWidget *>();
    tabs->setCurrentIndex(terminal ? 1 : 0);
    QWidget *pane = terminal ? static_cast<QWidget *>(window.findChild<SerialTerminalWidget *>())
                            : static_cast<QWidget *>(window.findChild<SerialDebugWidget *>());
    QTextEdit *view = nullptr;
    for (auto *editor : pane->findChildren<QTextEdit *>())
        if (terminal || editor->isReadOnly()) { view = editor; break; }
    if (!view) return 3;
    // Match Tauri's benchmark: retain history, no optional auto-clear.
    for (auto *check : pane->findChildren<QCheckBox *>())
        if (check->text().contains(QStringLiteral("自动清空"))) check->setChecked(false);
    window.show();
    auto *serial = window.findChild<SerialManager *>();
    if (!serial->open(QString::fromLocal8Bit(argv[1]), 115200, QSerialPort::Data8,
                      QSerialPort::NoParity, QSerialPort::OneStop)) return 4;
    QJsonArray samples, gaps;
    QSet<QString> seen;
    QElapsedTimer clock;
    clock.start();
    qint64 previous = 0;
    QTimer probe;
    QObject::connect(&probe, &QTimer::timeout, [&] {
        const auto tick = clock.elapsed();
        gaps.append(double(tick - previous));
        previous = tick;
        QTextCursor cursor(view->document());
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                            qMin(8192, view->document()->characterCount() - 1));
        auto matches = QRegularExpression("P([0-9]{8}):([0-9]{13})").globalMatch(cursor.selectedText());
        const auto now = QDateTime::currentMSecsSinceEpoch();
        while (matches.hasNext()) {
            auto m = matches.next();
            if (!seen.contains(m.captured(1))) {
                seen.insert(m.captured(1));
                samples.append(QJsonObject{{"id", m.captured(1).toInt()},
                    {"latency_ms", double(now - m.captured(2).toLongLong())}});
            }
        }
    });
    probe.start(16);
    std::cout << "READY" << std::endl;
    QTimer::singleShot(QString::fromLocal8Bit(argv[3]).toInt() * 1000, [&] {
        serial->close();
        QJsonObject result{{"samples", samples}, {"timer_gaps_ms", gaps},
            {"rx_bytes", double(serial->rxBytes())}, {"connected_after_close", serial->isOpen()}};
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
        app.quit();
    });
    return app.exec();
}
