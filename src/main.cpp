#include "MainWindow.h"
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QColor>
#include "ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 须尽早设置，与 geekcom.desktop basename 一致（Wayland 下作为 app-id）
    QGuiApplication::setDesktopFileName(QStringLiteral("geekcom"));
    app.setApplicationName(QStringLiteral("GeekCOM"));
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName(QStringLiteral("GeekCOM"));
    // GNOME / Ubuntu Dock：与 .desktop 关联后任务栏使用 Icon=
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));

    auto &theme = ThemeManager::instance();
    theme.setAccent(QColor(0x00, 0x78, 0xd4), QColor(0x10, 0x6e, 0xbe));
    theme.loadSettings();
    theme.apply();

    MainWindow w;
    w.show();
    return app.exec();
}
