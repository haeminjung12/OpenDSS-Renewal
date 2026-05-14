#pragma once

#include <QColor>
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

ThemeColors colors(ThemeMode mode = ThemeMode::Dark);
QPalette palette(ThemeMode mode = ThemeMode::Dark);
QString defaultStyleSheet();
QString shellStyleSheet(ThemeMode mode = ThemeMode::Dark);
BrandingResources brandingResources();

}  // namespace desktop_app::theme
