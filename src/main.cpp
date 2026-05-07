#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GeekCOM");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("GeekCOM");

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
