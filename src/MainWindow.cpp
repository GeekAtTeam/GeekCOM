#include "MainWindow.h"
#include "SerialManager.h"
#include "SerialDebugWidget.h"
#include "SerialTerminalWidget.h"
#include "ThemeManager.h"

#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QMenuBar>
#include <QMenu>

namespace {

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

    setupMenu();
    setupUi();
    setupConnections();
    refreshStatusStyle();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenu()
{
    // Ubuntu/GNOME 下原生菜单常被挪到顶栏或直接消失，强制显示在窗口内
    menuBar()->setNativeMenuBar(false);
    auto *viewMenu = menuBar()->addMenu(tr("视图"));
    ThemeManager::instance().installMenu(viewMenu);
}

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

    m_statusLabel = new QLabel(tr("就绪"));
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    connect(m_serial, &SerialManager::dataReceived,  this, &MainWindow::onDataReceived);
    connect(m_serial, &SerialManager::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_serial, &SerialManager::portOpened,    this, &MainWindow::onPortOpened);
    connect(m_serial, &SerialManager::portClosed,    this, &MainWindow::onPortClosed);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
}

void MainWindow::onThemeChanged()
{
    refreshStatusStyle();
}

void MainWindow::refreshStatusStyle()
{
    auto &t = ThemeManager::instance();
    switch (m_statusKind) {
    case StatusKind::Connected:
        m_statusLabel->setStyleSheet(t.styleSuccessText());
        break;
    case StatusKind::Error:
        m_statusLabel->setStyleSheet(t.styleDangerText());
        break;
    case StatusKind::Disconnected:
    case StatusKind::Ready:
    default:
        m_statusLabel->setStyleSheet(t.styleMutedText());
        break;
    }
}

void MainWindow::onTabChanged(int index)
{
    if (m_serial->isOpen() && index != m_prevTabIndex) {
        QMessageBox::information(this, tr("提示"),
            tr("切换模式前请先关闭串口连接。"));
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
    m_statusLabel->setText(tr("错误: %1").arg(error));
    m_statusKind = StatusKind::Error;
    refreshStatusStyle();
}

void MainWindow::onPortOpened()
{
    m_statusLabel->setText(tr("已连接: %1").arg(m_serial->portName()));
    m_statusKind = StatusKind::Connected;
    refreshStatusStyle();
}

void MainWindow::onPortClosed()
{
    m_statusLabel->setText(tr("已断开"));
    m_statusKind = StatusKind::Disconnected;
    refreshStatusStyle();
}
