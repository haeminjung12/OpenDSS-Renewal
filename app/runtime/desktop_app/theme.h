#pragma once 
 
#include <QColor> 
#include <QIcon>
#include <QPalette> 
#include <QString> 
 
namespace desktop_app::theme { 

enum class ThemeMode {
    Dark,
    Light,
};

struct ThemeColors {
    QColor brandNavy;
    QColor brandRoyal;
    QColor brandAqua;
    QColor brandSky;
    QColor appBackground;
    QColor shellBackground;
    QColor surface;
    QColor elevatedSurface;
    QColor inputSurface;
    QColor viewerBackground;
    QColor borderSubtle;
    QColor borderDefault;
    QColor borderStrong;
    QColor textPrimary;
    QColor textDefault;
    QColor textMuted;
    QColor textSubtle;
    QColor textDisabled;
    QColor onBrand;
    QColor progressTrack;
    QColor statusSuccess;
    QColor statusSuccessBg;
    QColor statusInfo;
    QColor statusInfoBg;
    QColor statusWarning;
    QColor statusWarningBg;
    QColor statusError;
    QColor statusErrorBg;
    QColor statusDisabled;
    QColor statusDisabledBg;
    QColor railText;
    QColor railHoverBg;
    QColor railCheckedBg;
    QColor shellIconFg;
    QColor shellIconAccent;
    QColor buttonBg;
    QColor buttonHoverBg;
    QColor buttonPressedBg;
    QColor buttonBorder;
    QColor buttonDisabledBg;
    QColor selectionBg;
    QColor selectionBorder;
    QColor chipNeutralBg;
    QColor chipNeutralBorder;
    QColor chipNeutralText;
    QColor chipRunningBg;
    QColor chipRunningBorder;
    QColor chipRunningText;
    QColor chipWarningFill;
    QColor chipWarningBorder;
    QColor chipWarningText;
    QColor chipErrorFill;
    QColor chipErrorBorder;
    QColor chipErrorText;
    QColor chipInfoFill;
    QColor chipInfoBorder;
    QColor chipInfoText;
    QColor chipDisabledFill;
    QColor chipDisabledBorder;
    QColor chipDisabledText;
    QColor actionPrimaryBg;
    QColor actionPrimaryHoverBg;
    QColor actionPrimaryPressedBg;
    QColor actionPrimaryFg;
    QColor actionDangerBg;
    QColor actionDangerHoverBg;
    QColor actionDangerPressedBg;
    QColor actionDangerFg;
    QColor reviewClassZeroBg;
    QColor reviewClassZeroHoverBg;
    QColor reviewClassZeroPressedBg;
    QColor reviewClassZeroFg;
    QColor reviewClassOneBg;
    QColor reviewClassOneHoverBg;
    QColor reviewClassOnePressedBg;
    QColor reviewClassOneFg;
    QColor reviewClassTwoBg;
    QColor reviewClassTwoHoverBg;
    QColor reviewClassTwoPressedBg;
    QColor reviewClassTwoFg;
    QColor reviewExcludeBg;
    QColor reviewExcludeHoverBg;
    QColor reviewExcludePressedBg;
    QColor reviewExcludeFg;
    QColor lastDecisionThumbBg;
    QColor lastDecisionThumbBorder;
    QColor lastDecisionThumbText;
    QColor statusDotWarn;
    QColor statusDotError;

    QColor surfaceBackground;
    QColor elevatedBackground;
    QColor subtleBorder;
    QColor defaultBorder;
    QColor foreground;
    QColor mutedForeground;
    QColor accent;
    QColor running;
    QColor warning;
    QColor error;
    QColor armed;
    QColor info;
    QColor idle;
};

struct BrandingResources {
    QString primaryFullColor;
    QString primaryWhite;
    QString iconFullColor;
    QString iconPngFallback;
    QString smallIconFullColor;
};

struct ReviewClassColors {
    QColor fill;
    QColor hoverFill;
    QColor pressedFill;
    QColor foreground;
};

ThemeColors colors(ThemeMode mode = ThemeMode::Dark); 
QPalette palette(ThemeMode mode = ThemeMode::Dark); 
QString defaultStyleSheet(); 
QString shellStyleSheet(ThemeMode mode = ThemeMode::Dark); 
BrandingResources brandingResources(); 
ReviewClassColors reviewClassColors(const QString& classId, ThemeMode mode = ThemeMode::Dark);
ReviewClassColors reviewClassColorsForBase(const QColor& baseColor, ThemeMode mode = ThemeMode::Dark);
QString reviewClassButtonStyle(const QColor& baseColor, ThemeMode mode = ThemeMode::Dark);
QColor semanticClassColor(const QString& classId, ThemeMode mode = ThemeMode::Dark);
QColor semanticClassColorForBase(const QColor& baseColor, ThemeMode mode = ThemeMode::Dark);
QIcon semanticClassIcon(const QString& classId, ThemeMode mode = ThemeMode::Dark);
QIcon semanticClassIconForBase(const QColor& baseColor, ThemeMode mode = ThemeMode::Dark);
 
} // namespace desktop_app::theme
