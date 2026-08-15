#include "ThemeManager.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QPalette>
#include <QProcess>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#  include <QStyleHints>
#endif

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    m_colors = buildColors(Scheme::Dark);
    startSystemWatch();
}

void ThemeManager::setAccent(const QColor &accent, const QColor &accentHover)
{
    m_accent = accent;
    m_accentHover = accentHover;
}

void ThemeManager::loadSettings()
{
    QSettings s;
    const QString v = s.value(QStringLiteral("ui/themeMode"), QStringLiteral("system")).toString();
    if (v == QLatin1String("light"))
        m_mode = Mode::Light;
    else if (v == QLatin1String("dark"))
        m_mode = Mode::Dark;
    else
        m_mode = Mode::System;
}

void ThemeManager::saveSettings() const
{
    QSettings s;
    QString v = QStringLiteral("system");
    if (m_mode == Mode::Light)
        v = QStringLiteral("light");
    else if (m_mode == Mode::Dark)
        v = QStringLiteral("dark");
    s.setValue(QStringLiteral("ui/themeMode"), v);
}

void ThemeManager::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    saveSettings();
    apply();
    syncMenuActions();
}

void ThemeManager::apply()
{
    const Scheme scheme = resolveScheme();
    m_scheme = scheme;
    m_colors = buildColors(scheme);

    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setPalette(buildPalette(m_colors));
    qApp->setStyleSheet(buildStyleSheet(m_colors));
    emit themeChanged();
}

ThemeManager::Scheme ThemeManager::resolveScheme() const
{
    switch (m_mode) {
    case Mode::Light: return Scheme::Light;
    case Mode::Dark:  return Scheme::Dark;
    case Mode::System:
    default:
        return systemPrefersDark() ? Scheme::Dark : Scheme::Light;
    }
}

bool ThemeManager::systemPrefersDark() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto cs = QGuiApplication::styleHints()->colorScheme();
    if (cs == Qt::ColorScheme::Dark)
        return true;
    if (cs == Qt::ColorScheme::Light)
        return false;
#endif

#ifdef Q_OS_LINUX
    QProcess p;
    p.start(QStringLiteral("gsettings"),
            {QStringLiteral("get"),
             QStringLiteral("org.gnome.desktop.interface"),
             QStringLiteral("color-scheme")});
    if (p.waitForFinished(400)) {
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        if (out.contains(QLatin1String("prefer-dark")))
            return true;
        if (out.contains(QLatin1String("prefer-light")))
            return false;
    }
#endif

    const QColor w = QApplication::style()
                         ? QApplication::style()->standardPalette().color(QPalette::Window)
                         : QGuiApplication::palette().color(QPalette::Window);
    return w.lightness() < 128;
}

ThemeManager::Colors ThemeManager::buildColors(Scheme scheme) const
{
    Colors c;
    c.accent = m_accent;
    c.accentHover = m_accentHover;
    c.accentText = Qt::white;
    c.danger = QColor(0xcc, 0x00, 0x00);
    c.dangerHover = QColor(0xaa, 0x00, 0x00);
    c.dangerText = Qt::white;
    c.success = QColor(0x00, 0x99, 0x00);
    c.successHover = QColor(0x00, 0x77, 0x00);
    c.warning = QColor(0xff, 0x98, 0x00);

    // Log surfaces stay dark in both schemes (strategy A).
    c.logBg = QColor(0x1e, 0x1e, 0x1e);
    c.logFg = QColor(0xd4, 0xd4, 0xd4);
    c.logMuted = QColor(0x88, 0x88, 0x88);
    c.logAccent = QColor(0x56, 0x9c, 0xd6);
    c.logHex = QColor(0x9c, 0xdc, 0xfe);
    c.logTimestamp = QColor(0x56, 0x9c, 0xd6);

    if (scheme == Scheme::Light) {
        c.window = QColor(0xf3, 0xf3, 0xf3);
        c.windowText = QColor(0x1a, 0x1a, 0x1a);
        c.base = QColor(0xff, 0xff, 0xff);
        c.alternateBase = QColor(0xef, 0xef, 0xef);
        c.text = QColor(0x1a, 0x1a, 0x1a);
        c.textMuted = QColor(0x5c, 0x5c, 0x5c);
        c.button = QColor(0xe8, 0xe8, 0xe8);
        c.buttonText = QColor(0x1a, 0x1a, 0x1a);
        c.highlight = m_accent;
        c.highlightedText = Qt::white;
        c.placeholder = QColor(0x88, 0x88, 0x88);
        c.border = QColor(0xc8, 0xc8, 0xc8);
        c.borderFocus = m_accent;
        c.mid = QColor(0xd0, 0xd0, 0xd0);
        c.dark = QColor(0xb0, 0xb0, 0xb0);
        c.shadow = QColor(0x90, 0x90, 0x90);
        c.tooltipBase = QColor(0xff, 0xff, 0xf0);
        c.tooltipText = QColor(0x1a, 0x1a, 0x1a);

        c.tableHeaderBg = QColor(0xe8, 0xe8, 0xe8);
        c.tableHeaderFg = QColor(0x00, 0x66, 0xaa);
        c.rowRxBg = QColor(0xff, 0xff, 0xff);
        c.rowTxBg = QColor(0xf3, 0xf8, 0xec);
        c.rowFdBg = QColor(0xf5, 0xf0, 0xfa);
        c.rowErrorBg = QColor(0xfd, 0xeb, 0xeb);
        c.rowRxFg = QColor(0x00, 0x66, 0xaa);
        c.rowTxFg = QColor(0xb0, 0x71, 0x00);
        c.rowFdFg = QColor(0x7b, 0x1f, 0xa2);
        c.rowErrorFg = QColor(0xc6, 0x28, 0x28);
    } else {
        c.window = QColor(0x28, 0x28, 0x28);
        c.windowText = QColor(0xdc, 0xdc, 0xdc);
        c.base = QColor(0x1c, 0x1c, 0x1c);
        c.alternateBase = QColor(0x2d, 0x2d, 0x2d);
        c.text = QColor(0xdc, 0xdc, 0xdc);
        c.textMuted = QColor(0xaa, 0xaa, 0xaa);
        c.button = QColor(0x37, 0x37, 0x37);
        c.buttonText = QColor(0xdc, 0xdc, 0xdc);
        c.highlight = m_accent.darker(110);
        c.highlightedText = Qt::white;
        c.placeholder = QColor(0x78, 0x78, 0x78);
        c.border = QColor(0x44, 0x44, 0x44);
        c.borderFocus = m_accent;
        c.mid = QColor(0x3c, 0x3c, 0x3c);
        c.dark = QColor(0x23, 0x23, 0x23);
        c.shadow = QColor(0x14, 0x14, 0x14);
        c.tooltipBase = QColor(0x32, 0x32, 0x32);
        c.tooltipText = QColor(0xdc, 0xdc, 0xdc);

        c.tableHeaderBg = QColor(0x2d, 0x2d, 0x2d);
        c.tableHeaderFg = QColor(0x9c, 0xdc, 0xfe);
        c.rowRxBg = QColor(0x1e, 0x1e, 0x1e);
        c.rowTxBg = QColor(0x1e, 0x23, 0x18);
        c.rowFdBg = QColor(0x1f, 0x1b, 0x2e);
        c.rowErrorBg = QColor(0x3b, 0x17, 0x17);
        c.rowRxFg = QColor(0x9c, 0xdc, 0xfe);
        c.rowTxFg = QColor(0xf9, 0xa8, 0x25);
        c.rowFdFg = QColor(0xce, 0x93, 0xd8);
        c.rowErrorFg = QColor(0xf4, 0x43, 0x36);
    }
    return c;
}

QPalette ThemeManager::buildPalette(const Colors &c) const
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.windowText);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.alternateBase);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.button);
    p.setColor(QPalette::ButtonText, c.buttonText);
    p.setColor(QPalette::Highlight, c.highlight);
    p.setColor(QPalette::HighlightedText, c.highlightedText);
    p.setColor(QPalette::PlaceholderText, c.placeholder);
    p.setColor(QPalette::ToolTipBase, c.tooltipBase);
    p.setColor(QPalette::ToolTipText, c.tooltipText);
    p.setColor(QPalette::Mid, c.mid);
    p.setColor(QPalette::Dark, c.dark);
    p.setColor(QPalette::Shadow, c.shadow);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::BrightText, c.danger);
    return p;
}

QString ThemeManager::buildStyleSheet(const Colors &c) const
{
    QString qss = QStringLiteral(
        "QMainWindow, QDialog, QWidget {"
        "  font-family: \"Segoe UI\", \"PingFang SC\", \"Microsoft YaHei\", \"Noto Sans CJK SC\", sans-serif;"
        "  font-size: 13px;"
        "}"
        "QGroupBox {"
        "  border: 1px solid {{border}}; border-radius: 5px; margin-top: 10px; padding-top: 6px;"
        "  color: {{textMuted}}; font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 6px; }"
        "QTabWidget::pane { border: 1px solid {{border}}; }"
        "QTabBar::tab {"
        "  background: {{window}}; color: {{textMuted}}; padding: 7px 16px; border: 1px solid {{border}};"
        "  border-bottom: none; border-radius: 4px 4px 0 0; margin-right: 2px;"
        "}"
        "QTabBar::tab:selected { background: {{base}}; color: {{text}}; border-bottom-color: {{base}}; }"
        "QTabBar::tab:hover:!selected { color: {{text}}; }"
        "QSplitter::handle { background: {{border}}; }"
        "QScrollBar:vertical { background: {{window}}; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: {{mid}}; border-radius: 5px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: {{textMuted}}; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: {{window}}; height: 10px; border: none; }"
        "QScrollBar::handle:horizontal { background: {{mid}}; border-radius: 5px; min-width: 24px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {"
        "  background: {{base}}; border: 1px solid {{border}}; border-radius: 3px; color: {{text}};"
        "  padding: 2px 4px; selection-background-color: {{highlight}}; selection-color: white;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,"
        "QTextEdit:focus, QPlainTextEdit:focus { border-color: {{accent}}; }"
        "QPushButton {"
        "  background: {{button}}; color: {{buttonText}}; border: 1px solid {{border}};"
        "  border-radius: 4px; padding: 4px 12px;"
        "}"
        "QPushButton:hover { border-color: {{accent}}; }"
        "QPushButton:pressed { background: {{mid}}; }"
        "QPushButton:disabled { color: {{textMuted}}; }"
        "QTreeWidget, QTreeView, QListView {"
        "  background: {{base}}; alternate-background-color: {{alt}}; border: 1px solid {{border}}; color: {{text}};"
        "}"
        "QTreeWidget::item:selected, QTreeView::item:selected, QListView::item:selected {"
        "  background: {{highlight}}; color: white;"
        "}"
        "QHeaderView::section {"
        "  background: {{headerBg}}; color: {{headerFg}}; border: none; border-right: 1px solid {{border}};"
        "  border-bottom: 1px solid {{border}}; padding: 4px 6px; font-weight: bold;"
        "}"
        "QTableView, QTableWidget {"
        "  background: {{base}}; alternate-background-color: {{alt}}; gridline-color: {{border}}; color: {{text}};"
        "  border: 1px solid {{border}}; selection-background-color: {{highlight}}; selection-color: white;"
        "}"
        "QStatusBar { background: {{window}}; color: {{textMuted}}; }"
        "QCheckBox, QLabel, QRadioButton { color: {{text}}; }"
        "QToolTip { background: {{tipBase}}; color: {{tipText}}; border: 1px solid {{border}}; }"
        "QMenuBar { background: {{window}}; color: {{text}}; }"
        "QMenuBar::item:selected { background: {{mid}}; }"
        "QMenu { background: {{base}}; color: {{text}}; border: 1px solid {{border}}; }"
        "QMenu::item:selected { background: {{highlight}}; color: white; }");

    const auto sub = [&](const char *key, const QColor &color) {
        qss.replace(QLatin1String(key), css(color));
    };
    sub("{{border}}", c.border);
    sub("{{textMuted}}", c.textMuted);
    sub("{{text}}", c.text);
    sub("{{base}}", c.base);
    sub("{{window}}", c.window);
    sub("{{button}}", c.button);
    sub("{{buttonText}}", c.buttonText);
    sub("{{accent}}", c.accent);
    sub("{{mid}}", c.mid);
    sub("{{highlight}}", c.highlight);
    sub("{{headerBg}}", c.tableHeaderBg);
    sub("{{headerFg}}", c.tableHeaderFg);
    sub("{{alt}}", c.alternateBase);
    sub("{{tipBase}}", c.tooltipBase);
    sub("{{tipText}}", c.tooltipText);
    return qss;
}

QString ThemeManager::buttonStyle(const QColor &bg, const QColor &hover,
                                  const QColor &fg, int minHeight) const
{
    const QString minH = minHeight > 0
                             ? QStringLiteral(" min-height:%1px;").arg(minHeight)
                             : QString();
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; font-weight:bold; border:none;"
               " border-radius:4px; padding:4px 12px;%3 }"
               "QPushButton:hover { background:%4; }"
               "QPushButton:disabled { background:%5; color:%6; }")
        .arg(css(bg), css(fg), minH, css(hover), css(m_colors.mid), css(m_colors.textMuted));
}

QString ThemeManager::stylePrimaryButton(int minHeight) const
{
    return buttonStyle(m_colors.accent, m_colors.accentHover, m_colors.accentText, minHeight);
}

QString ThemeManager::styleSuccessButton(int minHeight) const
{
    return buttonStyle(m_colors.success, m_colors.successHover, m_colors.accentText, minHeight);
}

QString ThemeManager::styleDangerButton(int minHeight) const
{
    return buttonStyle(m_colors.danger, m_colors.dangerHover, m_colors.dangerText, minHeight);
}

QString ThemeManager::styleLogView(bool borderless) const
{
    return QStringLiteral("background:%1; color:%2;%3")
        .arg(css(m_colors.logBg), css(m_colors.logFg),
             borderless ? QStringLiteral(" border:none;") : QString());
}

QString ThemeManager::styleMutedText(bool bold) const
{
    return QStringLiteral("color:%1;%2")
        .arg(css(m_colors.textMuted), bold ? QStringLiteral(" font-weight:bold;") : QString());
}

QString ThemeManager::styleSuccessText(bool bold) const
{
    return QStringLiteral("color:%1;%2")
        .arg(css(m_colors.success), bold ? QStringLiteral(" font-weight:bold;") : QString());
}

QString ThemeManager::styleDangerText(bool bold) const
{
    return QStringLiteral("color:%1;%2")
        .arg(css(m_colors.danger), bold ? QStringLiteral(" font-weight:bold;") : QString());
}

QString ThemeManager::styleWarningText(bool bold) const
{
    return QStringLiteral("color:%1;%2")
        .arg(css(m_colors.warning), bold ? QStringLiteral(" font-weight:bold;") : QString());
}

void ThemeManager::installMenu(QMenu *parentMenu)
{
    if (!parentMenu)
        return;

    auto *appearance = parentMenu->addMenu(tr("外观"));
    m_modeGroup = new QActionGroup(appearance);
    m_modeGroup->setExclusive(true);

    m_actSystem = appearance->addAction(tr("跟随系统"));
    m_actLight = appearance->addAction(tr("浅色"));
    m_actDark = appearance->addAction(tr("深色"));
    for (QAction *a : {m_actSystem, m_actLight, m_actDark}) {
        a->setCheckable(true);
        m_modeGroup->addAction(a);
    }

    connect(m_actSystem, &QAction::triggered, this, [this] { setMode(Mode::System); });
    connect(m_actLight, &QAction::triggered, this, [this] { setMode(Mode::Light); });
    connect(m_actDark, &QAction::triggered, this, [this] { setMode(Mode::Dark); });
    syncMenuActions();
}

void ThemeManager::syncMenuActions()
{
    if (!m_actSystem)
        return;
    m_actSystem->setChecked(m_mode == Mode::System);
    m_actLight->setChecked(m_mode == Mode::Light);
    m_actDark->setChecked(m_mode == Mode::Dark);
}

void ThemeManager::startSystemWatch()
{
    auto *timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, [this] {
        if (m_mode != Mode::System)
            return;
        const Scheme now = resolveScheme();
        if (now != m_scheme)
            apply();
    });
    timer->start();
}
