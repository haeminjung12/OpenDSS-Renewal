#include "theme.h"

#include <QStringBuilder>

namespace desktop_app::theme {
namespace {

QString cssColor(const QColor& color) {
  return color.name(QColor::HexRgb);
}

QString rgba(const QColor& color, int alpha) {
  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

}  // namespace

ThemeColors colors(ThemeMode mode) {
  ThemeColors c;
  if (mode == ThemeMode::Light) {
    c.brandNavy = QColor("#0B1F5E");
    c.brandRoyal = QColor("#2563EB");
    c.brandAqua = QColor("#14B8A6");
    c.brandSky = QColor("#7DD3FC");
    c.appBackground = QColor("#E5E7EB");
    c.shellBackground = QColor("#D1D5DB");
    c.surface = QColor("#FFFFFF");
    c.elevatedSurface = QColor("#F8FAFC");
    c.inputSurface = QColor("#FFFFFF");
    c.viewerBackground = QColor("#0A0A0A");
    c.borderSubtle = QColor("#E5E7EB");
    c.borderDefault = QColor("#CBD5E1");
    c.borderStrong = QColor("#94A3B8");
    c.textPrimary = QColor("#0B1F5E");
    c.textDefault = QColor("#1A1D24");
    c.textMuted = QColor("#64748B");
    c.textSubtle = QColor("#6B7280");
    c.textDisabled = QColor("#94A3B8");
    c.onBrand = QColor("#FFFFFF");
    c.progressTrack = QColor("#E5E7EB");
    c.statusSuccess = QColor("#14B8A6");
    c.statusSuccessBg = QColor("#D1FAF5");
    c.statusInfo = QColor("#2563EB");
    c.statusInfoBg = QColor("#E0F2FE");
    c.statusWarning = QColor("#D97706");
    c.statusWarningBg = QColor("#FEF3C7");
    c.statusError = QColor("#DC2626");
    c.statusErrorBg = QColor("#FEE2E2");
    c.statusDisabled = QColor("#94A3B8");
    c.statusDisabledBg = QColor("#E5E7EB");
  } else {
    c.brandNavy = QColor("#0B1F5E");
    c.brandRoyal = QColor("#2563EB");
    c.brandAqua = QColor("#14B8A6");
    c.brandSky = QColor("#7DD3FC");
    c.appBackground = QColor("#0E0F12");
    c.shellBackground = QColor("#1B1E24");
    c.surface = QColor("#14161B");
    c.elevatedSurface = QColor("#1A1D24");
    c.inputSurface = QColor("#1A1D24");
    c.viewerBackground = QColor("#0A0A0A");
    c.borderSubtle = QColor("#1F232B");
    c.borderDefault = QColor("#262A33");
    c.borderStrong = QColor("#4B5563");
    c.textPrimary = QColor("#E8EAEE");
    c.textDefault = QColor("#E8EAEE");
    c.textMuted = QColor("#94A3B8");
    c.textSubtle = QColor("#6B7280");
    c.textDisabled = QColor("#64748B");
    c.onBrand = QColor("#FFFFFF");
    c.progressTrack = QColor("#1F232B");
    c.statusSuccess = QColor("#14B8A6");
    c.statusSuccessBg = QColor("#173631");
    c.statusInfo = QColor("#7DD3FC");
    c.statusInfoBg = QColor("#172D39");
    c.statusWarning = QColor("#F59E0B");
    c.statusWarningBg = QColor("#3A2A11");
    c.statusError = QColor("#EF4444");
    c.statusErrorBg = QColor("#3A181A");
    c.statusDisabled = QColor("#64748B");
    c.statusDisabledBg = QColor("#14161B");
  }

  c.surfaceBackground = c.surface;
  c.elevatedBackground = c.elevatedSurface;
  c.subtleBorder = c.borderSubtle;
  c.defaultBorder = c.borderDefault;
  c.foreground = c.textDefault;
  c.mutedForeground = c.textMuted;
  c.accent = c.brandRoyal;
  c.running = c.statusSuccess;
  c.warning = c.statusWarning;
  c.error = c.statusError;
  c.armed = c.brandAqua;
  c.info = c.statusInfo;
  c.idle = c.statusDisabled;
  return c;
}

QPalette palette(ThemeMode mode) {
  const auto c = colors(mode);

  QPalette p;
  p.setColor(QPalette::Window, c.appBackground);
  p.setColor(QPalette::WindowText, c.textDefault);
  p.setColor(QPalette::Base, c.inputSurface);
  p.setColor(QPalette::AlternateBase, c.elevatedSurface);
  p.setColor(QPalette::Text, c.textDefault);
  p.setColor(QPalette::Button, c.elevatedSurface);
  p.setColor(QPalette::ButtonText, c.textDefault);
  p.setColor(QPalette::BrightText, c.statusError);
  p.setColor(QPalette::Highlight, c.brandRoyal);
  p.setColor(QPalette::HighlightedText, c.onBrand);
  p.setColor(QPalette::ToolTipBase, c.elevatedSurface);
  p.setColor(QPalette::ToolTipText, c.textDefault);
  p.setColor(QPalette::PlaceholderText, c.textMuted);
  p.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDisabled);
  p.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);

  return p;
}

QString defaultStyleSheet() {
  const auto c = colors(ThemeMode::Dark);

  return QString()
    % "QWidget { background: " % cssColor(c.appBackground)
    % "; color: " % cssColor(c.textDefault)
    % "; font-size: 13px; }\n"
    % "QFrame, QGroupBox { background: " % cssColor(c.surface)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 8px; }\n"
    % "QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: "
    % cssColor(c.inputSurface)
    % "; color: " % cssColor(c.textDefault)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 5px; min-height: 28px; selection-background-color: "
    % cssColor(c.brandRoyal) % "; }\n"
    % "QPushButton { background: " % cssColor(c.elevatedSurface)
    % "; color: " % cssColor(c.textDefault)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 5px; min-height: 28px; padding: 4px 10px; }\n"
    % "QPushButton:hover, QComboBox:hover, QLineEdit:hover { border-color: "
    % cssColor(c.brandRoyal) % "; }\n"
    % "QPushButton:pressed { background: " % cssColor(c.borderSubtle) % "; }\n"
    % "QPushButton:disabled, QLineEdit:disabled, QComboBox:disabled { color: "
    % cssColor(c.textDisabled)
    % "; background: " % cssColor(c.surface)
    % "; border-color: " % cssColor(c.borderSubtle) % "; }\n";
}

QString shellStyleSheet(ThemeMode mode) {
  const auto c = colors(mode);
  const bool light = mode == ThemeMode::Light;
  const QColor shell = c.shellBackground;
  const QColor shellText = light ? c.textPrimary : c.onBrand;
  const QColor shellMuted = light ? c.textMuted : QColor("#D1D5DB");
  const QColor shellBorder = light ? QColor("#B8C0CC") : QColor("#2B3038");
  const QColor railText = c.onBrand;
  const QColor buttonBg = light ? c.elevatedSurface : QColor("#252936");
  const QColor buttonHover = light ? QColor("#E5E7EB") : QColor("#303743");
  const QColor chipBg = light ? c.elevatedSurface : QColor("#19202A");
  const QColor chipBorder = light ? c.borderDefault : QColor("#3A4352");
  const QColor chipText = light ? c.textDefault : QColor("#F1F5F9");
  const QColor chipMuted = light ? c.textMuted : QColor("#CBD5E1");
  const QColor menuBg = light ? c.surface : QColor("#111318");
  const QColor menuBorder = light ? c.borderDefault : QColor("#303743");
  const QColor menuHover = light ? QColor("#E0F2FE") : QColor("#243447");
  const QColor menuDisabled = light ? c.textDisabled : QColor("#6F7D91");
  const QColor hudBg = QColor("#1A1D24");

  QString style;
  style.reserve(20000);

  style += QString()
    % "QMainWindow, QWidget#CentralWidget, QWidget#OpenDssShellContent, QStackedWidget#OpenDssWorkspaceStack,\n"
    % "QWidget#LiveWorkspace, QWidget#CameraWorkspace, QWidget#ModelWorkspace, QWidget#DatasetWorkspace, QWidget#TrainerWorkspace, QWidget#ValidatorWorkspace, QWidget#ReportsWorkspace, QWidget#SettingsWorkspace, QSplitter#MainSplitter, QSplitter#CameraWorkspaceSplitter,\n"
    % "QScrollArea#LiveRightMetricsScrollArea, QScrollArea#CameraRightSettingsScrollArea,\n"
    % "QScrollArea#ModelWorkspaceDetailScrollArea, QScrollArea#DatasetWorkspaceGridScrollArea, QScrollArea#TrainerWorkspaceLeftScrollArea, QScrollArea#TrainerWorkspaceRightScrollArea, QScrollArea#ValidatorWorkspaceLeftScrollArea, QScrollArea#ValidatorWorkspaceRightScrollArea, QScrollArea#ReportsWorkspaceDetailScrollArea, QScrollArea#SettingsWorkspaceScrollArea,\n"
    % "QScrollArea#LiveRightMetricsScrollArea > QWidget, QScrollArea#CameraRightSettingsScrollArea > QWidget,\n"
    % "QScrollArea#ModelWorkspaceDetailScrollArea > QWidget, QScrollArea#DatasetWorkspaceGridScrollArea > QWidget, QScrollArea#TrainerWorkspaceLeftScrollArea > QWidget, QScrollArea#TrainerWorkspaceRightScrollArea > QWidget, QScrollArea#ValidatorWorkspaceLeftScrollArea > QWidget, QScrollArea#ValidatorWorkspaceRightScrollArea > QWidget, QScrollArea#ReportsWorkspaceDetailScrollArea > QWidget, QScrollArea#SettingsWorkspaceScrollArea > QWidget,\n"
    % "QWidget#LiveRightMetricsStack, QWidget#CameraRightSettingsStack, QWidget#ModelWorkspaceDetailStack, QWidget#TrainerWorkspaceRightStack, QWidget#ValidatorWorkspaceLeftStack, QWidget#ValidatorWorkspaceRightStack, QWidget#ReportsWorkspaceDetailStack, QWidget#SettingsWorkspaceStack {\n"
    % "  background: " % cssColor(c.appBackground) % ";\n"
    % "  color: " % cssColor(c.textDefault) % ";\n"
    % "  font-size: 13px;\n"
    % "  font-family: Inter, \"Segoe UI\", Arial, sans-serif;\n"
    % "}\n"
    % "QFrame#OpenDssNavigationRail { background: " % cssColor(shell)
    % "; border-right: 1px solid " % cssColor(shellBorder) % "; }\n"
    % "QLabel#OpenDssRailLogo { background: " % cssColor(c.onBrand) % "; border-radius: 8px; }\n"
    % "QPushButton[railButton=\"true\"] { background: transparent; border: 0; border-radius: 5px; color: "
    % cssColor(railText) % "; min-width: 40px; max-width: 40px; min-height: 40px; max-height: 40px; padding: 0; font-weight: 600; }\n"
    % "QPushButton[railButton=\"true\"]::menu-indicator { width: 0; }\n"
    % "QPushButton[railButton=\"true\"]:hover, QPushButton[railButton=\"true\"]:checked { background: "
    % rgba(c.onBrand, 30) % "; color: " % cssColor(c.onBrand) % "; }\n"
    % "QPushButton[railButton=\"true\"]:checked { border-left: 3px solid " % cssColor(c.brandAqua) % "; }\n"
    % "QFrame#OpenDssHeader, QFrame#OpenDssStatusStrip { background: " % cssColor(shell) % "; border: 0; }\n"
    % "QFrame#OpenDssHeader { border-bottom: 1px solid " % cssColor(shellBorder) % "; min-height: 44px; max-height: 44px; }\n"
    % "QFrame#OpenDssStatusStrip { border-top: 1px solid " % cssColor(shellBorder) % "; min-height: 28px; max-height: 28px; }\n"
    % "QFrame#OpenDssStatusStrip QLabel { color: " % cssColor(shellMuted) % "; }\n"
    % "QLabel#OpenDssHeaderProductTitle { color: " % cssColor(shellText) % "; font-weight: 650; font-size: 14px; }\n"
    % "QLabel#OpenDssHeaderWorkspaceTitle { color: " % cssColor(shellMuted) % "; font-weight: 500; font-size: 14px; }\n"
    % "QLabel[statusChip=\"true\"] { background: " % cssColor(chipBg)
    % "; border: 1px solid " % cssColor(chipBorder)
    % "; border-radius: 8px; padding: 3px 9px 3px 8px; color: " % cssColor(chipText)
    % "; font-family: Consolas, \"Cascadia Mono\", monospace; font-size: 11px; }\n"
    % "QLabel[chipTone=\"running\"] { color: " % cssColor(chipText)
    % "; background: " % cssColor(chipBg) % "; border-color: " % cssColor(c.statusSuccess)
    % "; }\n"
    % "QLabel[chipTone=\"warn\"] { color: " % cssColor(chipText)
    % "; background: " % cssColor(chipBg) % "; border-color: " % cssColor(c.statusWarning)
    % "; }\n"
    % "QLabel[chipTone=\"error\"] { color: " % cssColor(chipText)
    % "; background: " % cssColor(chipBg) % "; border-color: " % cssColor(c.statusError)
    % "; }\n"
    % "QLabel[chipTone=\"info\"] { color: " % cssColor(chipMuted)
    % "; background: " % cssColor(chipBg) % "; border-color: " % cssColor(c.statusInfo)
    % "; }\n"
    % "QLabel[chipTone=\"disabled\"] { color: " % cssColor(chipMuted)
    % "; background: " % cssColor(chipBg) % "; border-color: " % cssColor(chipBorder)
    % "; }\n"
    % "QPushButton, QToolButton { background: " % cssColor(buttonBg)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 5px; color: " % cssColor(c.textDefault) % "; padding: 6px 12px; }\n"
    % "QToolButton[headerIcon=\"true\"] { min-width: 34px; max-width: 34px; min-height: 30px; max-height: 30px; padding: 0; }\n"
    % "QFrame#OpenDssHeader QToolButton[headerIcon=\"true\"] { background: "
    % (light ? QStringLiteral("rgba(148, 163, 184, 18)") : QStringLiteral("rgba(255, 255, 255, 18)"))
    % "; border-color: "
    % (light ? QStringLiteral("rgba(148, 163, 184, 72)") : QStringLiteral("rgba(229, 231, 235, 52)"))
    % "; }\n"
    % "QPushButton:hover, QToolButton:hover { background: " % cssColor(buttonHover) % "; border-color: " % cssColor(c.brandRoyal) % "; }\n"
    % "QPushButton:disabled, QToolButton:disabled { color: " % cssColor(c.textDisabled)
    % "; background: " % cssColor(c.statusDisabledBg) % "; border-color: " % cssColor(c.borderSubtle) % "; }\n"
    % "QPushButton#CameraStartButton { background: " % cssColor(c.statusInfo)
    % "; border-color: " % cssColor(c.statusInfo) % "; color: " % cssColor(c.onBrand)
    % "; min-width: 138px; font-weight: 650; }\n"
    % "QPushButton#PipelineStartButton { background: " % cssColor(c.brandAqua)
    % "; border-color: " % cssColor(c.brandAqua) % "; color: " % cssColor(c.onBrand)
    % "; min-width: 138px; font-weight: 650; }\n"
    % "QPushButton#PipelineStopButton { background: " % cssColor(c.statusError)
    % "; border-color: " % cssColor(c.statusError) % "; color: " % cssColor(c.onBrand)
    % "; min-width: 118px; font-weight: 650; }\n"
    % "QPushButton#LiveTriggerSafeButton { background: " % cssColor(c.elevatedSurface)
    % "; border-color: " % cssColor(c.borderDefault) % "; color: " % cssColor(c.textDefault) % "; }\n"
    % "QPushButton[primaryAction=\"true\"] { background: " % cssColor(c.brandRoyal)
    % "; border-color: " % cssColor(c.brandRoyal) % "; color: " % cssColor(c.onBrand)
    % "; font-weight: 650; }\n"
    % "QFrame[panel=\"true\"] { background: " % cssColor(c.surface)
    % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 8px; color: " % cssColor(c.textDefault) % "; }\n"
    % "QWidget[viewerCanvas=\"true\"], QFrame#LiveViewerStack, QFrame#CameraViewerFrame, QWidget#CameraViewerPattern { background: " % cssColor(c.viewerBackground) % "; border: 0; }\n"
    % "QWidget#LiveViewerHudOverlay, QWidget#CameraViewerHudOverlay { background: transparent; }\n"
    % "QLabel[hudPill=\"true\"] { background: " % rgba(hudBg, 205)
    % "; border: 1px solid " % rgba(c.borderStrong, 165)
    % "; border-radius: 4px; color: #E8EAEE; padding: 4px 8px; font-size: 11px; font-family: Consolas, \"Cascadia Mono\", monospace; }\n"
    % "QLabel#LiveViewerEmptyState, QLabel#CameraViewerEmptyState { color: #94A3B8; font-size: 12px; font-family: Consolas, \"Cascadia Mono\", monospace; background: transparent; border: 0; border-radius: 5px; padding: 8px 12px; }\n"
    % "QFrame#LiveViewerHudToolbar, QFrame#CameraViewerHudToolbar { color: #E8EAEE; background: " % rgba(hudBg, 220)
    % "; border: 1px solid " % rgba(c.borderStrong, 165) % "; border-radius: 5px; font-size: 11px; }\n"
    % "QToolButton[viewerTool=\"true\"] { background: transparent; border: 0; color: #FFFFFF; min-width: 28px; max-width: 28px; min-height: 24px; max-height: 24px; padding: 0; }\n"
    % "QToolButton[viewerTool=\"true\"]:checked, QToolButton[viewerTool=\"true\"]:hover { background: "
    % rgba(c.brandSky, 35) % "; }\n"
    % "QFrame#LiveImageOverlayStatusStrip { background: rgba(10, 10, 10, 205); border-top: 1px solid rgba(75, 85, 99, 130); }\n";

  style += QString()
    % "QLabel[panelTitle=\"true\"] { color: " % cssColor(c.textPrimary) % "; font-size: 12px; font-weight: 700; letter-spacing: 1px; text-transform: uppercase; }\n"
    % "QLabel[panelSubtitle=\"true\"], QLabel[mutedText=\"true\"] { color: " % cssColor(c.textMuted) % "; font-size: 11px; }\n"
    % "QLabel[metricValue=\"true\"] { color: " % cssColor(c.textPrimary) % "; font-size: 22px; font-weight: 650; font-family: Consolas, \"Cascadia Mono\", monospace; }\n"
    % "QLabel[metricLabel=\"true\"] { color: " % cssColor(c.textMuted) % "; font-size: 10px; font-weight: 650; }\n"
    % "QLabel[statusDot=\"true\"] { color: " % cssColor(c.brandAqua) % "; font-size: 14px; font-weight: 700; }\n"
    % "QLabel[statusPill=\"true\"] { background: " % cssColor(c.elevatedSurface)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 8px; padding: 2px 7px; color: " % cssColor(c.textDefault)
    % "; font-size: 10px; font-family: Consolas, \"Cascadia Mono\", monospace; }\n"
    % "QFrame#LiveLastDecisionCard { background: " % cssColor(c.elevatedSurface)
    % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 5px; }\n"
    % "QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: " % cssColor(c.inputSurface)
    % "; color: " % cssColor(c.textDefault)
    % "; border: 1px solid " % cssColor(c.borderDefault)
    % "; border-radius: 3px; padding: 4px 6px; selection-background-color: " % cssColor(c.brandRoyal) % "; }\n"
    % "QCheckBox { color: " % cssColor(c.textDefault) % "; }\n";

  style += QString()
    % "QScrollArea#LiveImageView { background: transparent; border: 0; }\n"
    % "QScrollArea#LiveImageView > QWidget, QLabel#LiveImageLabel { background: transparent; }\n"
    % "QTableWidget#ModelWorkspaceRegistryTable { background: " % cssColor(c.surface)
    % "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: "
    % cssColor(c.elevatedSurface) % "; selection-color: " % cssColor(c.textDefault) % "; }\n"
    % "QTableWidget#ModelWorkspaceRegistryTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle)
    % "; padding: 8px 6px; }\n"
    % "QTableWidget#ModelWorkspaceRegistryTable::item:selected { border-left: 3px solid " % cssColor(c.statusInfo) % "; }\n"
    % "QPushButton[datasetFilterRow=\"true\"] { text-align: left; background: transparent; border: 0; border-radius: 5px; padding: 6px 10px; }\n"
    % "QPushButton[datasetFilterRow=\"true\"]:checked { background: " % cssColor(c.elevatedSurface) % "; border-left: 3px solid " % cssColor(c.brandAqua) % "; }\n"
    % "QPushButton[datasetFilterRow=\"true\"][chipTone=\"running\"] { color: " % cssColor(c.statusSuccess) % "; }\n"
    % "QPushButton[datasetFilterRow=\"true\"][chipTone=\"warn\"] { color: " % cssColor(c.statusWarning) % "; }\n"
    % "QPushButton[datasetFilterRow=\"true\"][chipTone=\"error\"] { color: " % cssColor(c.statusError) % "; }\n";

  style += QString()
    % "QFrame[datasetTile=\"true\"] { background: " % cssColor(c.elevatedSurface) % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 5px; }\n"
    % "QFrame[datasetTile=\"true\"][selected=\"true\"] { border: 2px solid " % cssColor(c.brandAqua) % "; }\n"
    % "QLabel[datasetThumb=\"true\"] { background: " % cssColor(c.viewerBackground) % "; border: 1px solid " % cssColor(c.borderStrong) % "; border-radius: 4px; color: " % cssColor(c.textMuted) % "; font-family: Consolas, \"Cascadia Mono\", monospace; }\n"
    % "QFrame#DatasetWorkspacePreviewFrame { background: " % cssColor(c.viewerBackground) % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 8px; }\n"
    % "QTableWidget#DatasetWorkspaceCropListTable { background: " % cssColor(c.surface) % "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " % cssColor(c.elevatedSurface) % "; selection-color: " % cssColor(c.textDefault) % "; }\n"
    % "QTableWidget#DatasetWorkspaceCropListTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) % "; padding: 7px 6px; }\n"
    % "QTableWidget#DatasetWorkspaceCropListTable::item:selected { border-left: 3px solid " % cssColor(c.brandAqua) % "; }\n";

  style += QString()
    % "QFrame[trainerCheckRow=\"true\"] { background: " % cssColor(c.elevatedSurface) % "; border: 1px solid " % cssColor(c.borderSubtle) % "; border-radius: 5px; }\n"
    % "QTableWidget#TrainerRecentRunsTable { background: " % cssColor(c.surface) % "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " % cssColor(c.elevatedSurface) % "; selection-color: " % cssColor(c.textDefault) % "; }\n"
    % "QTableWidget#TrainerRecentRunsTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) % "; padding: 7px 6px; }\n"
    % "QProgressBar#TrainerWorkspaceProgressBar { background: " % cssColor(c.progressTrack) % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 4px; color: " % cssColor(c.textMuted) % "; text-align: center; min-height: 18px; }\n"
    % "QProgressBar#TrainerWorkspaceProgressBar::chunk { background: " % cssColor(c.brandRoyal) % "; border-radius: 3px; }\n"
    % "QProgressBar#ValidatorWorkspaceProgressBar { background: " % cssColor(c.progressTrack) % "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 4px; color: " % cssColor(c.textMuted) % "; text-align: center; min-height: 18px; }\n"
    % "QProgressBar#ValidatorWorkspaceProgressBar::chunk { background: " % cssColor(c.brandRoyal) % "; border-radius: 3px; }\n"
    % "QTableWidget#ValidatorWorkspaceConfusionTable { background: " % cssColor(c.surface) % "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " % cssColor(c.elevatedSurface) % "; selection-color: " % cssColor(c.textDefault) % "; }\n"
    % "QTableWidget#ValidatorWorkspaceConfusionTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) % "; padding: 7px 6px; }\n"
    % "QPlainTextEdit#ValidatorWorkspaceLogTextEdit { font-family: Consolas, \"Cascadia Mono\", monospace; font-size: 11px; }\n";

  style += QString()
    % "QListWidget#ReportsWorkspaceRunList { background: " % cssColor(c.surface) % "; border: 0; outline: 0; }\n"
    % "QListWidget#ReportsWorkspaceRunList::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) % "; padding: 10px 12px; color: " % cssColor(c.textDefault) % "; }\n"
    % "QListWidget#ReportsWorkspaceRunList::item:selected { background: " % cssColor(c.elevatedSurface) % "; border-left: 3px solid " % cssColor(c.statusInfo) % "; }\n"
    % "QPlainTextEdit#ReportsWorkspaceSessionLogTextEdit { font-family: Consolas, \"Cascadia Mono\", monospace; font-size: 11px; }\n"
    % "QSplitter::handle, QSplitter#MainSplitter::handle, QSplitter#CameraWorkspaceSplitter::handle { background: " % cssColor(c.appBackground) % "; width: 8px; }\n"
    % "QMenu { background: " % cssColor(menuBg)
    % "; color: " % cssColor(c.textDefault)
    % "; border: 1px solid " % cssColor(menuBorder)
    % "; padding: 6px; }\n"
    % "QMenu::item { color: " % cssColor(c.textDefault)
    % "; background: transparent; padding: 7px 28px 7px 12px; min-width: 150px; }\n"
    % "QMenu::item:selected { background: " % cssColor(menuHover)
    % "; color: " % cssColor(c.textDefault) % "; }\n"
    % "QMenu::item:disabled { color: " % cssColor(menuDisabled)
    % "; background: transparent; }\n"
    % "QMenu::separator { height: 1px; background: " % cssColor(menuBorder)
    % "; margin: 5px 6px; }\n"
    % "QComboBox QAbstractItemView { background: " % cssColor(menuBg)
    % "; color: " % cssColor(c.textDefault)
    % "; border: 1px solid " % cssColor(menuBorder)
    % "; selection-background-color: " % cssColor(menuHover)
    % "; selection-color: " % cssColor(c.textDefault)
    % "; outline: 0; }\n"
    % "QComboBox QAbstractItemView::item { min-height: 24px; padding: 6px 12px; }\n"
    % "QComboBox QAbstractItemView::item:hover { background: " % cssColor(menuHover)
    % "; color: " % cssColor(c.textDefault) % "; }\n"
    % "QComboBox QAbstractItemView::item:selected { background: " % cssColor(menuHover)
    % "; color: " % cssColor(c.textDefault) % "; }\n"
    % "QComboBox QAbstractItemView::item:disabled { color: " % cssColor(menuDisabled)
    % "; background: transparent; }\n"
    % "QComboBox QAbstractItemView::item:selected:disabled { color: " % cssColor(menuDisabled)
    % "; background: transparent; }\n"
    % "QComboBox QAbstractItemView QScrollBar { background: transparent; }\n";

  return style;
}

BrandingResources brandingResources() {
  return {
    QStringLiteral(":/branding/opendss-primary-full-color.svg"),
    QStringLiteral(":/branding/opendss-primary-white.svg"),
    QStringLiteral(":/branding/opendss-icon-full-color.svg"),
    QStringLiteral(":/branding/opendss-icon-512.png"),
    QStringLiteral(":/branding/opendss-small-icon-full-color.svg"),
  };
}

}  // namespace desktop_app::theme
