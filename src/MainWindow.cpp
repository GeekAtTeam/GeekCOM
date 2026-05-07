#include "MainWindow.h"
#include "SerialManager.h"
#include "SerialDebugWidget.h"
#include "SerialTerminalWidget.h"

#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new SerialManager(this))
{
    setWindowTitle("GeekCOM - 串口调试工具 v1.0");
    setMinimumSize(900, 600);
    resize(1100, 700);

    setupUi();
    setupConnections();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    m_debugWidget    = new SerialDebugWidget(m_serial, this);
    m_terminalWidget = new SerialTerminalWidget(m_serial, this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_debugWidget,    "🔬  串口调试");
    m_tabWidget->addTab(m_terminalWidget, "💻  串口终端");
    setCentralWidget(m_tabWidget);

    // Status bar
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    connect(m_serial, &SerialManager::dataReceived,  this, &MainWindow::onDataReceived);
    connect(m_serial, &SerialManager::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_serial, &SerialManager::portOpened,    this, &MainWindow::onPortOpened);
    connect(m_serial, &SerialManager::portClosed,    this, &MainWindow::onPortClosed);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
}

void MainWindow::onTabChanged(int index)
{
    // If switching tabs while port is open, warn user
    if (m_serial->isOpen() && index != m_prevTabIndex) {
        QMessageBox::information(this, "提示",
            "切换模式前请先关闭串口连接。");
        m_tabWidget->setCurrentIndex(m_prevTabIndex);
        return;
    }
    m_prevTabIndex = index;
}

void MainWindow::onDataReceived(const QByteArray &data)
{
    int tab = m_tabWidget->currentIndex();
    if (tab == 0)
        m_debugWidget->onDataReceived(data);
    else
        m_terminalWidget->onDataReceived(data);
}

void MainWindow::onSerialError(const QString &error)
{
    m_statusLabel->setText("错误: " + error);
    m_statusLabel->setStyleSheet("color:red;");
}

void MainWindow::onPortOpened()
{
    m_statusLabel->setText(QString("已连接: %1").arg(m_serial->portName()));
    m_statusLabel->setStyleSheet("color:green;");
}

void MainWindow::onPortClosed()
{
    m_statusLabel->setText("已断开");
    m_statusLabel->setStyleSheet("color:#555;");
}
