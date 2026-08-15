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
    void onThemeChanged();

private:
    void setupUi();
    void setupMenu();
    void setupConnections();
    void refreshStatusStyle();

    SerialManager        *m_serial;
    SerialDebugWidget    *m_debugWidget;
    SerialTerminalWidget *m_terminalWidget;

    QTabWidget *m_tabWidget;
    QLabel     *m_statusLabel;

    int m_prevTabIndex = 0;
    enum class StatusKind { Ready, Connected, Error, Disconnected };
    StatusKind m_statusKind = StatusKind::Ready;
};
