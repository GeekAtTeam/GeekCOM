#pragma once
#include <QMainWindow>

class SerialManager;
class SerialDebugWidget;
class SerialTerminalWidget;
class QTabWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onTabChanged(int index);
    void onDataReceived(const QByteArray &data);
    void onSerialError(const QString &error);
    void onPortOpened();
    void onPortClosed();

private:
    void setupUi();
    void setupConnections();

    SerialManager        *m_serial;
    SerialDebugWidget    *m_debugWidget;
    SerialTerminalWidget *m_terminalWidget;

    QTabWidget *m_tabWidget;
    QLabel     *m_statusLabel;

    int m_prevTabIndex = 0;
};
