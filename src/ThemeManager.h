#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QAction;
class QActionGroup;
class QMenu;

/**
 * Central theme controller: System / Light / Dark.
 * Chrome follows the effective scheme; log/terminal surfaces stay on log* tokens.
 */
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };
    enum class Scheme { Light, Dark };

    struct Colors {
        QColor window;
        QColor windowText;
        QColor base;
        QColor alternateBase;
        QColor text;
        QColor textMuted;
        QColor button;
        QColor buttonText;
        QColor highlight;
        QColor highlightedText;
        QColor placeholder;
        QColor border;
        QColor borderFocus;
        QColor mid;
        QColor dark;
        QColor shadow;
        QColor tooltipBase;
        QColor tooltipText;

        QColor accent;
        QColor accentHover;
        QColor accentText;
        QColor accentMuted;
        QColor danger;
        QColor dangerHover;
        QColor dangerText;
        QColor success;
        QColor successHover;
        QColor warning;

        /** Optional product-specific tokens (GeekCAN bus lines). */
        QColor canHigh;
        QColor canLow;

        /** Always-dark code/log surfaces (strategy A). */
        QColor logBg;
        QColor logFg;
        QColor logMuted;
        QColor logAccent;
        QColor logHex;
        QColor logTimestamp;

        QColor tableHeaderBg;
        QColor tableHeaderFg;
        QColor rowRxBg;
        QColor rowTxBg;
        QColor rowFdBg;
        QColor rowErrorBg;
        QColor rowRxFg;
        QColor rowTxFg;
        QColor rowFdFg;
        QColor rowErrorFg;
    };

    static ThemeManager &instance();

    /**
     * Brand colors from the product logo. Call once before apply().
     * @param canHigh / canLow  optional CAN-H / CAN-L line colors (GeekCAN).
     */
    void setBrand(const QColor &accent, const QColor &accentHover,
                  const QColor &canHigh = QColor(),
                  const QColor &canLow = QColor());

    /** @deprecated use setBrand */
    void setAccent(const QColor &accent, const QColor &accentHover);


    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    Scheme effectiveScheme() const { return m_scheme; }
    const Colors &colors() const { return m_colors; }

    void apply();
    void loadSettings();
    void saveSettings() const;

    /** Add 「外观」 menu with System/Light/Dark actions (exclusive). */
    void installMenu(QMenu *parentMenu);

    // —— Style helpers (semantic, no raw hex at call sites) ——
    QString stylePrimaryButton(int minHeight = 0) const;
    QString styleSuccessButton(int minHeight = 0) const;
    QString styleDangerButton(int minHeight = 0) const;
    QString styleLogView(bool borderless = false) const;
    QString styleMutedText(bool bold = false) const;
    QString styleSuccessText(bool bold = false) const;
    QString styleDangerText(bool bold = false) const;
    QString styleWarningText(bool bold = false) const;
    QString css(const QColor &c) const { return c.name(QColor::HexRgb); }

signals:
    void themeChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);

    Scheme resolveScheme() const;
    bool systemPrefersDark() const;
    Colors buildColors(Scheme scheme) const;
    QPalette buildPalette(const Colors &c) const;
    QString buildStyleSheet(const Colors &c) const;
    QString buttonStyle(const QColor &bg, const QColor &hover,
                        const QColor &fg, int minHeight) const;
    void syncMenuActions();
    void startSystemWatch();

    Mode m_mode = Mode::System;
    Scheme m_scheme = Scheme::Dark;
    Colors m_colors;
    QColor m_accent{0xE4, 0x2C, 0x2C};
    QColor m_accentHover{0xC4, 0x25, 0x25};
    QColor m_canHigh;
    QColor m_canLow;

    QAction *m_actSystem = nullptr;
    QAction *m_actLight = nullptr;
    QAction *m_actDark = nullptr;
    QActionGroup *m_modeGroup = nullptr;
};
