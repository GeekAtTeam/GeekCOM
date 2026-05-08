#include "MainWindow.h"
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 须尽早设置，与 geekcom.desktop  basename 一致（Wayland 下作为 app-id）
    QGuiApplication::setDesktopFileName(QStringLiteral("geekcom"));
    app.setApplicationName(QStringLiteral("GeekCOM"));
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName(QStringLiteral("GeekCOM"));
    // GNOME / Ubuntu Dock：与 .desktop 关联后任务栏使用 Icon=
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));

    // Use Fusion style for consistent cross-platform look
    app.setStyle(QStyleFactory::create("Fusion"));

    // Optional dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText,      Qt::white);
    darkPalette.setColor(QPalette::Base,            QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase,   QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase,     Qt::white);
    darkPalette.setColor(QPalette::ToolTipText,     Qt::white);
    darkPalette.setColor(QPalette::Text,            Qt::white);
    darkPalette.setColor(QPalette::Button,          QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText,      Qt::white);
    darkPalette.setColor(QPalette::BrightText,      Qt::red);
    darkPalette.setColor(QPalette::Link,            QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);

    MainWindow w;
    w.show();
    return app.exec();
}
