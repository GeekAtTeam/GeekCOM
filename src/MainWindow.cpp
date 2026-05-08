#include "MainWindow.h"
#include "SerialManager.h"
#include "SerialDebugWidget.h"
#include "SerialTerminalWidget.h"

#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QIcon>
#include <QStyle>

namespace {

/** 优先使用系统图标主题（常见于 Linux），否则用 Qt 标准像素图，避免依赖 Emoji 字体。 */
QIcon tabIconThemed(const QString &iconName, QStyle::StandardPixmap fallback)
{
    QIcon themed = QIcon::fromTheme(iconName);
    if (!themed.isNull())
        return themed;
    return QApplication::style()->standardIcon(fallback);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new SerialManager(this))
{
    setWindowTitle("GeekCOM - 串口调试工具 v1.0");
    setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));
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
    m_tabWidget->setIconSize(QSize(16, 16));
    m_tabWidget->addTab(m_debugWidget,
                        tabIconThemed(QStringLiteral("accessories-text-editor"), QStyle::SP_FileDialogDetailedView),
                        tr("串口调试"));
    m_tabWidget->addTab(m_terminalWidget,
                        tabIconThemed(QStringLiteral("utilities-terminal"), QStyle::SP_CommandLink),
                        tr("串口终端"));
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
