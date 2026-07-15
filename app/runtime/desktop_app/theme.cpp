#include "theme.h" 
 
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStringBuilder> 
 
namespace desktop_app::theme { 
namespace { 
 
QString cssColor(const QColor& color) {
    return color.name(QColor::HexRgb);
}

QString rgba(const QColor& color, int alpha) { 
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha); 
} 

QString canonicalSemanticClassId(QString classId) {
    classId = classId.trimmed().toLower();
    if (classId == "1" || classId == "target" || classId == "hit" || classId == "hits" || classId == "single")
        return QStringLiteral("1");
    if (classId == "0" || classId == "non-target" || classId == "non_target" || classId == "waste" ||
        classId == "empty")
        return QStringLiteral("0");
    if (classId == "2" || classId == "more" || classId == "morethantwo" || classId == "non-target b" ||
        classId == "second non-target")
        return QStringLiteral("2");
    if (classId == "exclude" || classId == "excluded" || classId == "reject" || classId == "rejected")
        return QStringLiteral("exclude");
    return classId;
}

QColor readableForeground(const QColor& fill) {
    return qGray(fill.rgb()) >= 170 ? QColor("#111827") : QColor("#FFFFFF");
}

ReviewClassColors buildReviewClassColorsForBase(const QColor& baseColor) {
    const QColor fill = QColor(baseColor.name(QColor::HexRgb));
    return {fill, fill.darker(112), fill.darker(132), readableForeground(fill)};
}

ReviewClassColors buildReviewClassColors(const ThemeColors& c, const QString& classId) {
    const QString canonical = canonicalSemanticClassId(classId);
    if (canonical == "0")
        return {c.reviewClassZeroBg, c.reviewClassZeroHoverBg, c.reviewClassZeroPressedBg, c.reviewClassZeroFg};
    if (canonical == "1")
        return {c.reviewClassOneBg, c.reviewClassOneHoverBg, c.reviewClassOnePressedBg, c.reviewClassOneFg};
    if (canonical == "2")
        return {c.reviewClassTwoBg, c.reviewClassTwoHoverBg, c.reviewClassTwoPressedBg, c.reviewClassTwoFg};
    if (canonical == "exclude")
        return {c.reviewExcludeBg, c.reviewExcludeHoverBg, c.reviewExcludePressedBg, c.reviewExcludeFg};
    return {c.buttonBg, c.buttonHoverBg, c.buttonPressedBg, c.textDefault};
}

} // namespace 

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
        c.railText = QColor("#0F172A");
        c.railHoverBg = QColor("#FFFFFF");
        c.railCheckedBg = QColor("#DBEAFE");
        c.shellIconFg = QColor("#0F172A");
        c.shellIconAccent = QColor("#2563EB");
        c.buttonBg = QColor("#FFFFFF");
        c.buttonHoverBg = QColor("#F1F5F9");
        c.buttonPressedBg = QColor("#E2E8F0");
        c.buttonBorder = QColor("#94A3B8");
        c.buttonDisabledBg = QColor("#E5E7EB");
        c.selectionBg = QColor("#DBEAFE");
        c.selectionBorder = QColor("#2563EB");
        c.chipNeutralBg = QColor("#FFFFFF");
        c.chipNeutralBorder = QColor("#94A3B8");
        c.chipNeutralText = QColor("#0F172A");
        c.chipRunningBg = QColor("#DCFCE7");
        c.chipRunningBorder = QColor("#166534");
        c.chipRunningText = QColor("#166534");
        c.chipWarningFill = QColor("#FEF3C7");
        c.chipWarningBorder = QColor("#D97706");
        c.chipWarningText = QColor("#92400E");
        c.chipErrorFill = QColor("#FEE2E2");
        c.chipErrorBorder = QColor("#DC2626");
        c.chipErrorText = QColor("#991B1B");
        c.chipInfoFill = QColor("#DBEAFE");
        c.chipInfoBorder = QColor("#2563EB");
        c.chipInfoText = QColor("#1D4ED8");
        c.chipDisabledFill = QColor("#E5E7EB");
        c.chipDisabledBorder = QColor("#94A3B8");
        c.chipDisabledText = QColor("#64748B");
        c.actionPrimaryBg = QColor("#1D4ED8");
        c.actionPrimaryHoverBg = QColor("#1E40AF");
        c.actionPrimaryPressedBg = QColor("#1E3A8A");
        c.actionPrimaryFg = QColor("#FFFFFF");
        c.actionDangerBg = QColor("#B91C1C");
        c.actionDangerHoverBg = QColor("#991B1B");
        c.actionDangerPressedBg = QColor("#7F1D1D");
        c.actionDangerFg = QColor("#FFFFFF");
        c.reviewClassZeroBg = QColor("#FF0000");
        c.reviewClassZeroHoverBg = QColor("#D00000");
        c.reviewClassZeroPressedBg = QColor("#A80000");
        c.reviewClassZeroFg = QColor("#FFFFFF");
        c.reviewClassOneBg = QColor("#00FF00");
        c.reviewClassOneHoverBg = QColor("#00D000");
        c.reviewClassOnePressedBg = QColor("#00A800");
        c.reviewClassOneFg = QColor("#111827");
        c.reviewClassTwoBg = QColor("#0000FF");
        c.reviewClassTwoHoverBg = QColor("#0000D0");
        c.reviewClassTwoPressedBg = QColor("#0000A8");
        c.reviewClassTwoFg = QColor("#FFFFFF");
        c.reviewExcludeBg = QColor("#4B5563");
        c.reviewExcludeHoverBg = QColor("#334155");
        c.reviewExcludePressedBg = QColor("#1E293B");
        c.reviewExcludeFg = QColor("#FFFFFF");
        c.lastDecisionThumbBg = c.viewerBackground;
        c.lastDecisionThumbBorder = c.borderStrong;
        c.lastDecisionThumbText = c.textMuted;
        c.statusDotWarn = QColor("#D97706");
        c.statusDotError = QColor("#DC2626");
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
        c.railText = QColor("#F8FAFC");
        c.railHoverBg = QColor("#364152");
        c.railCheckedBg = QColor("#1F2A37");
        c.shellIconFg = QColor("#F8FAFC");
        c.shellIconAccent = QColor("#7DD3FC");
        c.buttonBg = QColor("#2B3442");
        c.buttonHoverBg = QColor("#475569");
        c.buttonPressedBg = QColor("#1E293B");
        c.buttonBorder = QColor("#94A3B8");
        c.buttonDisabledBg = QColor("#1A1F27");
        c.selectionBg = QColor("#27466E");
        c.selectionBorder = QColor("#7DD3FC");
        c.chipNeutralBg = QColor("#1E2733");
        c.chipNeutralBorder = QColor("#475569");
        c.chipNeutralText = QColor("#F8FAFC");
        c.chipRunningBg = QColor("#0F332B");
        c.chipRunningBorder = QColor("#22C55E");
        c.chipRunningText = QColor("#DCFCE7");
        c.chipWarningFill = QColor("#3C2A0F");
        c.chipWarningBorder = QColor("#F59E0B");
        c.chipWarningText = QColor("#FDE68A");
        c.chipErrorFill = QColor("#3A181A");
        c.chipErrorBorder = QColor("#F87171");
        c.chipErrorText = QColor("#FECACA");
        c.chipInfoFill = QColor("#112A46");
        c.chipInfoBorder = QColor("#60A5FA");
        c.chipInfoText = QColor("#DBEAFE");
        c.chipDisabledFill = QColor("#1A1F27");
        c.chipDisabledBorder = QColor("#334155");
        c.chipDisabledText = QColor("#94A3B8");
        c.actionPrimaryBg = QColor("#1D4ED8");
        c.actionPrimaryHoverBg = QColor("#1E40AF");
        c.actionPrimaryPressedBg = QColor("#1E3A8A");
        c.actionPrimaryFg = QColor("#FFFFFF");
        c.actionDangerBg = QColor("#B91C1C");
        c.actionDangerHoverBg = QColor("#991B1B");
        c.actionDangerPressedBg = QColor("#7F1D1D");
        c.actionDangerFg = QColor("#FFFFFF");
        c.reviewClassZeroBg = QColor("#FF0000");
        c.reviewClassZeroHoverBg = QColor("#D00000");
        c.reviewClassZeroPressedBg = QColor("#A80000");
        c.reviewClassZeroFg = QColor("#FFFFFF");
        c.reviewClassOneBg = QColor("#00FF00");
        c.reviewClassOneHoverBg = QColor("#00D000");
        c.reviewClassOnePressedBg = QColor("#00A800");
        c.reviewClassOneFg = QColor("#111827");
        c.reviewClassTwoBg = QColor("#0000FF");
        c.reviewClassTwoHoverBg = QColor("#0000D0");
        c.reviewClassTwoPressedBg = QColor("#0000A8");
        c.reviewClassTwoFg = QColor("#FFFFFF");
        c.reviewExcludeBg = QColor("#4B5563");
        c.reviewExcludeHoverBg = QColor("#334155");
        c.reviewExcludePressedBg = QColor("#1E293B");
        c.reviewExcludeFg = QColor("#FFFFFF");
        c.lastDecisionThumbBg = c.viewerBackground;
        c.lastDecisionThumbBorder = c.borderStrong;
        c.lastDecisionThumbText = c.textMuted;
        c.statusDotWarn = QColor("#F59E0B");
        c.statusDotError = QColor("#F87171");
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
    p.setColor(QPalette::Highlight, c.selectionBg);
    p.setColor(QPalette::HighlightedText, c.textDefault);
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

    return QString() % "QWidget { background: " % cssColor(c.appBackground) % "; color: " % cssColor(c.textDefault) %
           "; font-size: 13px; }\n" % "QFrame, QGroupBox { background: " % cssColor(c.surface) %
           "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 8px; }\n" %
           "QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: " %
           cssColor(c.inputSurface) % "; color: " % cssColor(c.textDefault) % "; border: 1px solid " %
           cssColor(c.borderDefault) % "; border-radius: 5px; min-height: 28px; selection-background-color: " %
           cssColor(c.brandRoyal) % "; }\n" % "QPushButton { background: " % cssColor(c.elevatedSurface) % "; color: " %
           cssColor(c.textDefault) % "; border: 1px solid " % cssColor(c.borderDefault) %
           "; border-radius: 5px; min-height: 28px; padding: 4px 10px; }\n" %
           "QPushButton:hover, QComboBox:hover, QLineEdit:hover { border-color: " % cssColor(c.brandRoyal) % "; }\n" %
           "QPushButton:pressed { background: " % cssColor(c.borderSubtle) % "; }\n" %
           "QPushButton:disabled, QLineEdit:disabled, QComboBox:disabled { color: " % cssColor(c.textDisabled) %
           "; background: " % cssColor(c.surface) % "; border-color: " % cssColor(c.borderSubtle) % "; }\n";
} 
 
QString shellStyleSheet(ThemeMode mode) { 
    const auto c = colors(mode);
    const ReviewClassColors classZeroColors = buildReviewClassColors(c, "0");
    const ReviewClassColors classOneColors = buildReviewClassColors(c, "1");
    const ReviewClassColors classTwoColors = buildReviewClassColors(c, "2");
    const ReviewClassColors excludeColors = buildReviewClassColors(c, "exclude");
    const bool light = mode == ThemeMode::Light;
    const QColor shell = c.shellBackground;
    const QColor shellText = light ? c.textPrimary : c.onBrand;
    const QColor shellMuted = light ? c.textMuted : QColor("#D1D5DB");
    const QColor shellBorder = light ? QColor("#B8C0CC") : QColor("#2B3038");
    const QColor menuBg = light ? c.surface : QColor("#111318");
    const QColor menuBorder = light ? c.borderDefault : QColor("#303743");
    const QColor menuHover = light ? QColor("#E0F2FE") : QColor("#243447");
    const QColor menuDisabled = light ? c.textDisabled : QColor("#6F7D91");
    const QColor hudBg = QColor("#1A1D24");

    QString style;
    style.reserve(20000);

    style +=
        QString() %
        "QMainWindow, QWidget#CentralWidget, QWidget#OpenDssShellContent, QStackedWidget#OpenDssWorkspaceStack,\n" %
        "QWidget#LiveWorkspace, QWidget#CameraWorkspace, QWidget#ModelWorkspace, QWidget#DatasetWorkspace, "
        "QWidget#TrainerWorkspace, QWidget#ValidatorWorkspace, QWidget#ReportsWorkspace, QWidget#SettingsWorkspace, "
        "QSplitter#MainSplitter, QSplitter#CameraWorkspaceSplitter,\n" %
        "QScrollArea#LiveRightMetricsScrollArea, QScrollArea#CameraRightSettingsScrollArea,\n" %
        "QScrollArea#ModelWorkspaceDetailScrollArea, QScrollArea#DatasetWorkspaceGridScrollArea, "
        "QScrollArea#TrainerWorkspaceLeftScrollArea, QScrollArea#TrainerWorkspaceRightScrollArea, "
        "QScrollArea#ValidatorWorkspaceLeftScrollArea, QScrollArea#ValidatorWorkspaceRightScrollArea, "
        "QScrollArea#ReportsWorkspaceDetailScrollArea, QScrollArea#SettingsWorkspaceScrollArea,\n" %
        "QScrollArea#LiveRightMetricsScrollArea > QWidget, QScrollArea#CameraRightSettingsScrollArea > QWidget,\n" %
        "QScrollArea#ModelWorkspaceDetailScrollArea > QWidget, QScrollArea#DatasetWorkspaceGridScrollArea > QWidget, "
        "QScrollArea#TrainerWorkspaceLeftScrollArea > QWidget, QScrollArea#TrainerWorkspaceRightScrollArea > QWidget, "
        "QScrollArea#ValidatorWorkspaceLeftScrollArea > QWidget, QScrollArea#ValidatorWorkspaceRightScrollArea > "
        "QWidget, QScrollArea#ReportsWorkspaceDetailScrollArea > QWidget, QScrollArea#SettingsWorkspaceScrollArea > "
        "QWidget,\n" %
        "QWidget#LiveRightMetricsStack, QWidget#CameraRightSettingsStack, QWidget#ModelWorkspaceDetailStack, "
        "QWidget#TrainerWorkspaceRightStack, QWidget#ValidatorWorkspaceLeftStack, "
        "QWidget#ValidatorWorkspaceRightStack, QWidget#ReportsWorkspaceDetailStack, QWidget#SettingsWorkspaceStack "
        "{\n" %
        "  background: " % cssColor(c.appBackground) % ";\n" % "  color: " % cssColor(c.textDefault) % ";\n" %
        "  font-size: 13px;\n" % "  font-family: Inter, \"Segoe UI\", Arial, sans-serif;\n" % "}\n" %
        "QFrame#OpenDssNavigationRail { background: " % cssColor(shell) % "; border-right: 1px solid " %
        cssColor(shellBorder) % "; }\n" % "QLabel#OpenDssRailLogo { background: " % cssColor(c.onBrand) %
        "; border-radius: 8px; }\n" %
        "QPushButton[railButton=\"true\"] { background: transparent; border: 0; border-left: 3px solid transparent; "
        "border-radius: 5px; color: " %
        cssColor(c.railText) %
        "; min-width: 40px; max-width: 40px; min-height: 40px; max-height: 40px; padding: 0; font-weight: 600; }\n" %
        "QPushButton[railButton=\"true\"]::menu-indicator { width: 0; }\n" %
        "QPushButton[railButton=\"true\"]:hover { background: " % cssColor(c.railHoverBg) % "; color: " %
        cssColor(c.railText) % "; }\n" %
        "QPushButton[railButton=\"true\"]:checked { background: " % cssColor(c.railCheckedBg) % "; color: " %
        cssColor(c.railText) % "; border-left: 3px solid " % cssColor(c.selectionBorder) % "; }\n" %
        "QFrame#OpenDssHeader, QFrame#OpenDssStatusStrip { background: " % cssColor(shell) % "; border: 0; }\n" %
        "QFrame#OpenDssHeader { border-bottom: 1px solid " % cssColor(shellBorder) %
        "; min-height: 44px; max-height: 44px; }\n" % "QFrame#OpenDssStatusStrip { border-top: 1px solid " %
        cssColor(shellBorder) % "; min-height: 28px; max-height: 28px; }\n" %
        "QFrame#OpenDssStatusStrip QLabel { color: " % cssColor(shellMuted) % "; }\n" %
        "QLabel#OpenDssHeaderProductTitle { color: " % cssColor(shellText) %
        "; font-weight: 650; font-size: 14px; }\n" % "QLabel#OpenDssHeaderWorkspaceTitle { color: " %
        cssColor(shellMuted) % "; font-weight: 500; font-size: 14px; }\n" %
        "QLabel[statusChip=\"true\"] { background: " % cssColor(c.chipNeutralBg) % "; border: 1px solid " %
        cssColor(c.chipNeutralBorder) % "; border-radius: 8px; padding: 3px 9px 3px 8px; color: " %
        cssColor(c.chipNeutralText) %
        "; font-family: Consolas, \"Cascadia Mono\", monospace; font-size: 11px; }\n" %
        "QLabel[chipTone=\"running\"] { color: " % cssColor(c.chipRunningText) % "; background: " %
        cssColor(c.chipRunningBg) % "; border-color: " % cssColor(c.chipRunningBorder) % "; }\n" %
        "QLabel[chipTone=\"warn\"] { color: " % cssColor(c.chipWarningText) % "; background: " %
        cssColor(c.chipWarningFill) % "; border-color: " % cssColor(c.chipWarningBorder) % "; }\n" %
        "QLabel[chipTone=\"error\"] { color: " % cssColor(c.chipErrorText) % "; background: " %
        cssColor(c.chipErrorFill) % "; border-color: " % cssColor(c.chipErrorBorder) % "; }\n" %
        "QLabel[chipTone=\"info\"] { color: " % cssColor(c.chipInfoText) % "; background: " %
        cssColor(c.chipInfoFill) % "; border-color: " % cssColor(c.chipInfoBorder) % "; }\n" %
        "QLabel[chipTone=\"disabled\"] { color: " % cssColor(c.chipDisabledText) % "; background: " %
        cssColor(c.chipDisabledFill) % "; border-color: " % cssColor(c.chipDisabledBorder) % "; }\n" %
        "QPushButton, QToolButton { background: " % cssColor(c.buttonBg) % "; border: 1px solid " %
        cssColor(c.buttonBorder) % "; border-radius: 5px; color: " % cssColor(c.textDefault) %
        "; padding: 6px 12px; }\n" %
        "QToolButton[headerIcon=\"true\"] { min-width: 34px; max-width: 34px; min-height: 30px; max-height: 30px; "
        "padding: 0; }\n" %
        "QFrame#OpenDssHeader QToolButton[headerIcon=\"true\"] { background: " %
        (light ? QStringLiteral("rgba(148, 163, 184, 18)") : QStringLiteral("rgba(255, 255, 255, 18)")) %
        "; border-color: " %
        (light ? QStringLiteral("rgba(148, 163, 184, 72)") : QStringLiteral("rgba(229, 231, 235, 52)")) % "; }\n" %
        "QFrame#OpenDssHeader QToolButton[headerIcon=\"true\"]:hover { background: " % cssColor(c.railHoverBg) %
        "; border-color: " % cssColor(c.selectionBorder) % "; }\n" %
        "QFrame#OpenDssHeader QToolButton[headerIcon=\"true\"]:pressed { background: " % cssColor(c.railCheckedBg) %
        "; border-color: " % cssColor(c.selectionBorder) % "; }\n" %
        "QPushButton:hover, QToolButton:hover { background: " % cssColor(c.buttonHoverBg) % "; border-color: " %
        cssColor(c.brandRoyal) % "; }\n" % "QPushButton:disabled, QToolButton:disabled { color: " %
        cssColor(c.textDisabled) % "; background: " % cssColor(c.buttonDisabledBg) % "; border-color: " %
        cssColor(c.borderSubtle) % "; }\n" % "QPushButton:pressed, QToolButton:pressed { background: " %
        cssColor(c.buttonPressedBg) % "; }\n" %
        "QPushButton#CameraStartButton, QPushButton#PipelineStartButton { background: " %
        cssColor(c.actionPrimaryBg) % "; border-color: " % cssColor(c.actionPrimaryBg) % "; color: " %
        cssColor(c.actionPrimaryFg) % "; min-width: 138px; font-weight: 650; }\n" %
        "QPushButton#CameraStartButton:hover, QPushButton#PipelineStartButton:hover { background: " %
        cssColor(c.actionPrimaryHoverBg) % "; border-color: " % cssColor(c.actionPrimaryHoverBg) % "; color: " %
        cssColor(c.actionPrimaryFg) % "; }\n" %
        "QPushButton#CameraStartButton:pressed, QPushButton#PipelineStartButton:pressed { background: " %
        cssColor(c.actionPrimaryPressedBg) % "; border-color: " % cssColor(c.actionPrimaryPressedBg) % "; color: " %
        cssColor(c.actionPrimaryFg) % "; }\n" %
        "QPushButton#PipelineStopButton { background: " % cssColor(c.actionDangerBg) % "; border-color: " %
        cssColor(c.actionDangerBg) % "; color: " % cssColor(c.actionDangerFg) %
        "; min-width: 118px; font-weight: 650; }\n" %
        "QPushButton#PipelineStopButton:hover { background: " % cssColor(c.actionDangerHoverBg) % "; border-color: " %
        cssColor(c.actionDangerHoverBg) % "; color: " % cssColor(c.actionDangerFg) % "; }\n" %
        "QPushButton#PipelineStopButton:pressed { background: " % cssColor(c.actionDangerPressedBg) % "; border-color: "
        % cssColor(c.actionDangerPressedBg) % "; color: " % cssColor(c.actionDangerFg) % "; }\n" %
        "QPushButton#LiveTriggerSafeButton { background: " %
        cssColor(c.elevatedSurface) % "; border-color: " % cssColor(c.borderDefault) % "; color: " %
        cssColor(c.textDefault) % "; }\n" % "QPushButton[primaryAction=\"true\"] { background: " %
        cssColor(c.actionPrimaryBg) % "; border-color: " % cssColor(c.actionPrimaryBg) % "; color: " %
        cssColor(c.actionPrimaryFg) % "; font-weight: 650; }\n" %
        "QPushButton[primaryAction=\"true\"]:hover { background: " % cssColor(c.actionPrimaryHoverBg) %
        "; border-color: " % cssColor(c.actionPrimaryHoverBg) % "; color: " % cssColor(c.actionPrimaryFg) % "; }\n" %
        "QPushButton[primaryAction=\"true\"]:pressed { background: " % cssColor(c.actionPrimaryPressedBg) %
        "; border-color: " % cssColor(c.actionPrimaryPressedBg) % "; color: " % cssColor(c.actionPrimaryFg) % "; }\n" %
        "QPushButton[reviewClassId=\"0\"] { background: " % cssColor(classZeroColors.fill) % "; border-color: " %
        cssColor(classZeroColors.fill) % "; color: " % cssColor(classZeroColors.foreground) %
        "; font-weight: 650; }\n" % "QPushButton[reviewClassId=\"0\"]:hover { background: " %
        cssColor(classZeroColors.hoverFill) % "; border-color: " % cssColor(classZeroColors.hoverFill) % "; color: " %
        cssColor(classZeroColors.foreground) % "; }\n" %
        "QPushButton[reviewClassId=\"0\"]:pressed { background: " % cssColor(classZeroColors.pressedFill) %
        "; border-color: " % cssColor(classZeroColors.pressedFill) % "; color: " %
        cssColor(classZeroColors.foreground) % "; }\n" % "QPushButton[reviewClassId=\"1\"] { background: " %
        cssColor(classOneColors.fill) % "; border-color: " % cssColor(classOneColors.fill) % "; color: " %
        cssColor(classOneColors.foreground) % "; font-weight: 650; }\n" %
        "QPushButton[reviewClassId=\"1\"]:hover { background: " % cssColor(classOneColors.hoverFill) %
        "; border-color: " % cssColor(classOneColors.hoverFill) % "; color: " %
        cssColor(classOneColors.foreground) % "; }\n" %
        "QPushButton[reviewClassId=\"1\"]:pressed { background: " % cssColor(classOneColors.pressedFill) %
        "; border-color: " % cssColor(classOneColors.pressedFill) % "; color: " %
        cssColor(classOneColors.foreground) % "; }\n" % "QPushButton[reviewClassId=\"2\"] { background: " %
        cssColor(classTwoColors.fill) % "; border-color: " % cssColor(classTwoColors.fill) % "; color: " %
        cssColor(classTwoColors.foreground) % "; font-weight: 650; }\n" %
        "QPushButton[reviewClassId=\"2\"]:hover { background: " % cssColor(classTwoColors.hoverFill) %
        "; border-color: " % cssColor(classTwoColors.hoverFill) % "; color: " %
        cssColor(classTwoColors.foreground) % "; }\n" %
        "QPushButton[reviewClassId=\"2\"]:pressed { background: " % cssColor(classTwoColors.pressedFill) %
        "; border-color: " % cssColor(classTwoColors.pressedFill) % "; color: " %
        cssColor(classTwoColors.foreground) % "; }\n" % "QPushButton[reviewClassId=\"exclude\"] { background: " %
        cssColor(excludeColors.fill) % "; border-color: " % cssColor(excludeColors.fill) % "; color: " %
        cssColor(excludeColors.foreground) % "; font-weight: 650; }\n" %
        "QPushButton[reviewClassId=\"exclude\"]:hover { background: " % cssColor(excludeColors.hoverFill) %
        "; border-color: " % cssColor(excludeColors.hoverFill) % "; color: " %
        cssColor(excludeColors.foreground) % "; }\n" %
        "QPushButton[reviewClassId=\"exclude\"]:pressed { background: " % cssColor(excludeColors.pressedFill) %
        "; border-color: " % cssColor(excludeColors.pressedFill) % "; color: " %
        cssColor(excludeColors.foreground) % "; }\n" %
        "QPushButton#CameraStartButton:disabled, QPushButton#PipelineStartButton:disabled, "
        "QPushButton#PipelineStopButton:disabled, QPushButton[primaryAction=\"true\"]:disabled, "
        "QPushButton[reviewClassId=\"0\"]:disabled, QPushButton[reviewClassId=\"1\"]:disabled, "
        "QPushButton[reviewClassId=\"2\"]:disabled, QPushButton[reviewClassId=\"exclude\"]:disabled { background: " %
        cssColor(c.buttonDisabledBg) % "; border-color: " % cssColor(c.borderSubtle) % "; color: " %
        cssColor(c.textDisabled) % "; }\n" % "QFrame[panel=\"true\"] { background: " % cssColor(c.surface) %
        "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 8px; color: " % cssColor(c.textDefault) %
        "; }\n" %
        "QWidget[viewerCanvas=\"true\"], QFrame#LiveViewerStack, QFrame#CameraViewerFrame, QWidget#CameraViewerPattern "
        "{ background: " %
        cssColor(c.viewerBackground) % "; border: 0; }\n" %
        "QWidget#LiveViewerHudOverlay, QWidget#CameraViewerHudOverlay { background: transparent; }\n" %
        "QLabel[hudPill=\"true\"] { background: " % rgba(hudBg, 205) % "; border: 1px solid " %
        rgba(c.borderStrong, 165) %
        "; border-radius: 4px; color: #E8EAEE; padding: 4px 8px; font-size: 11px; font-family: Consolas, \"Cascadia "
        "Mono\", monospace; }\n" %
        "QLabel#LiveViewerEmptyState, QLabel#CameraViewerEmptyState { color: #94A3B8; font-size: 12px; font-family: "
        "Consolas, \"Cascadia Mono\", monospace; background: transparent; border: 0; border-radius: 5px; padding: 8px "
        "12px; }\n" %
        "QFrame#LiveViewerHudToolbar, QFrame#CameraViewerHudToolbar { color: #E8EAEE; background: " % rgba(hudBg, 220) %
        "; border: 1px solid " % rgba(c.borderStrong, 165) % "; border-radius: 5px; font-size: 11px; }\n" %
        "QToolButton[viewerTool=\"true\"] { background: transparent; border: 0; color: #FFFFFF; min-width: 28px; "
        "max-width: 28px; min-height: 24px; max-height: 24px; padding: 0; }\n" %
        "QToolButton[viewerTool=\"true\"]:checked, QToolButton[viewerTool=\"true\"]:hover { background: " %
        rgba(c.brandSky, 35) % "; }\n" %
        "QFrame#LiveImageOverlayStatusStrip { background: rgba(10, 10, 10, 205); border-top: 1px solid rgba(75, 85, "
        "99, 130); }\n";

    style += QString() % "QLabel[panelTitle=\"true\"] { color: " % cssColor(c.textPrimary) %
             "; font-size: 12px; font-weight: 700; letter-spacing: 1px; text-transform: uppercase; }\n" %
             "QLabel[panelSubtitle=\"true\"], QLabel[mutedText=\"true\"] { color: " % cssColor(c.textMuted) %
             "; font-size: 11px; }\n" % "QLabel[metricValue=\"true\"] { color: " % cssColor(c.textPrimary) %
             "; font-size: 22px; font-weight: 650; font-family: Consolas, \"Cascadia Mono\", monospace; }\n" %
             "QLabel[metricLabel=\"true\"] { color: " % cssColor(c.textMuted) %
             "; font-size: 10px; font-weight: 650; }\n" % "QLabel[statusDot=\"true\"] { color: " %
             cssColor(c.brandAqua) % "; font-size: 14px; font-weight: 700; }\n" %
             "QLabel[statusDot=\"true\"][statusTone=\"warn\"] { color: " % cssColor(c.statusDotWarn) % "; }\n" % 
             "QLabel[statusDot=\"true\"][statusTone=\"error\"] { color: " % cssColor(c.statusDotError) % "; }\n" % 
             "QLabel[statusPill=\"true\"] { background: " % cssColor(c.elevatedSurface) % "; border: 1px solid " % 
             cssColor(c.borderDefault) % "; border-radius: 8px; padding: 2px 7px; color: " % cssColor(c.textDefault) % 
             "; font-size: 10px; font-family: Consolas, \"Cascadia Mono\", monospace; }\n" % 
             "QLabel[statusPill=\"true\"][semanticClassId=\"0\"] { background: " % cssColor(c.chipInfoFill) %
             "; border-color: " % cssColor(c.chipInfoBorder) % "; color: " % cssColor(c.chipInfoText) % "; }\n" %
             "QLabel[statusPill=\"true\"][semanticClassId=\"1\"] { background: " % cssColor(c.chipRunningBg) %
             "; border-color: " % cssColor(c.chipRunningBorder) % "; color: " % cssColor(c.chipRunningText) %
             "; }\n" % "QLabel[statusPill=\"true\"][semanticClassId=\"2\"] { background: " %
             cssColor(c.chipWarningFill) % "; border-color: " % cssColor(c.chipWarningBorder) % "; color: " %
             cssColor(c.chipWarningText) % "; }\n" %
             "QLabel[statusPill=\"true\"][semanticClassId=\"exclude\"] { background: " %
             cssColor(c.chipDisabledFill) % "; border-color: " % cssColor(c.chipDisabledBorder) % "; color: " %
             cssColor(c.chipDisabledText) % "; }\n" %
             "QFrame#LiveLastDecisionCard { background: " % cssColor(c.elevatedSurface) % "; border: 1px solid " % 
             cssColor(c.borderDefault) % "; border-radius: 5px; }\n" % 
             "QLabel#LiveLastDecisionThumbnail { background: " % cssColor(c.lastDecisionThumbBg) % "; color: " % 
             cssColor(c.lastDecisionThumbText) % "; border: 1px solid " % cssColor(c.lastDecisionThumbBorder) % 
             "; border-radius: 3px; font-size: 10px; }\n" %
             "QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: " %
             cssColor(c.inputSurface) % "; color: " % cssColor(c.textDefault) % "; border: 1px solid " %
             cssColor(c.borderDefault) % "; border-radius: 3px; padding: 4px 6px; selection-background-color: " %
             cssColor(c.brandRoyal) % "; }\n" % "QCheckBox { color: " % cssColor(c.textDefault) % "; }\n";

    style += QString() % "QScrollArea#LiveImageView { background: transparent; border: 0; }\n" %
             "QScrollArea#LiveImageView > QWidget, QLabel#LiveImageLabel { background: transparent; }\n" %
             "QTableWidget#ModelWorkspaceRegistryTable { background: " % cssColor(c.surface) %
             "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " %
             cssColor(c.selectionBg) % "; selection-color: " % cssColor(c.textDefault) % "; }\n" %
             "QTableWidget#ModelWorkspaceRegistryTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) %
             "; padding: 8px 6px; }\n" %
             "QTableWidget#ModelWorkspaceRegistryTable::item:selected { border-left: 3px solid " %
             cssColor(c.selectionBorder) % "; background: " % cssColor(c.selectionBg) % "; }\n" %
             "QPushButton[datasetFilterRow=\"true\"] { text-align: left; background: transparent; border: 0; "
             "border-radius: 5px; padding: 6px 10px; }\n" %
             "QPushButton[datasetFilterRow=\"true\"]:hover { background: " % cssColor(c.buttonHoverBg) % "; }\n" %
             "QPushButton[datasetFilterRow=\"true\"]:checked { background: " % cssColor(c.selectionBg) %
             "; border-left: 3px solid " % cssColor(c.selectionBorder) % "; }\n" %
             "QPushButton[datasetFilterRow=\"true\"][chipTone=\"running\"] { color: " % cssColor(c.statusSuccess) %
             "; }\n" % "QPushButton[datasetFilterRow=\"true\"][chipTone=\"warn\"] { color: " %
             cssColor(c.statusWarning) % "; }\n" %
             "QPushButton[datasetFilterRow=\"true\"][chipTone=\"error\"] { color: " % cssColor(c.statusError) % "; }\n";

    style += QString() % "QFrame[datasetTile=\"true\"] { background: " % cssColor(c.elevatedSurface) %
             "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 5px; }\n" %
             "QFrame[datasetTile=\"true\"][selected=\"true\"] { border: 2px solid " % cssColor(c.selectionBorder) %
             "; }\n" %
             "QLabel[datasetThumb=\"true\"] { background: " % cssColor(c.viewerBackground) % "; border: 1px solid " %
             cssColor(c.borderStrong) % "; border-radius: 4px; color: " % cssColor(c.textMuted) %
             "; font-family: Consolas, \"Cascadia Mono\", monospace; }\n" %
             "QFrame#DatasetWorkspacePreviewFrame { background: " % cssColor(c.viewerBackground) %
             "; border: 1px solid " % cssColor(c.borderDefault) % "; border-radius: 8px; }\n" %
             "QTableWidget#DatasetWorkspaceCropListTable { background: " % cssColor(c.surface) %
             "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " %
             cssColor(c.selectionBg) % "; selection-color: " % cssColor(c.textDefault) % "; }\n" %
             "QTableWidget#DatasetWorkspaceCropListTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) %
             "; padding: 7px 6px; }\n" %
             "QTableWidget#DatasetWorkspaceCropListTable::item:selected { border-left: 3px solid " %
             cssColor(c.selectionBorder) % "; background: " % cssColor(c.selectionBg) % "; }\n";

    style += QString() % "QFrame[trainerCheckRow=\"true\"] { background: " % cssColor(c.elevatedSurface) %
             "; border: 1px solid " % cssColor(c.borderSubtle) % "; border-radius: 5px; }\n" %
             "QTableWidget#TrainerRecentRunsTable { background: " % cssColor(c.surface) %
             "; border: 0; gridline-color: " % cssColor(c.borderSubtle) % "; selection-background-color: " %
             cssColor(c.selectionBg) % "; selection-color: " % cssColor(c.textDefault) % "; }\n" %
             "QTableWidget#TrainerRecentRunsTable::item { border-bottom: 1px solid " % cssColor(c.borderSubtle) %
             "; padding: 7px 6px; }\n" % "QProgressBar#TrainerWorkspaceProgressBar { background: " %
             cssColor(c.progressTrack) % "; border: 1px solid " % cssColor(c.borderDefault) %
             "; border-radius: 4px; color: " % cssColor(c.textMuted) % "; text-align: center; min-height: 18px; }\n" %
             "QProgressBar#TrainerWorkspaceProgressBar::chunk { background: " % cssColor(c.brandRoyal) %
             "; border-radius: 3px; }\n" % "QProgressBar#ValidatorWorkspaceProgressBar { background: " %
             cssColor(c.progressTrack) % "; border: 1px solid " % cssColor(c.borderDefault) %
             "; border-radius: 4px; color: " % cssColor(c.textMuted) % "; text-align: center; min-height: 18px; }\n" %
             "QProgressBar#ValidatorWorkspaceProgressBar::chunk { background: " % cssColor(c.brandRoyal) %
             "; border-radius: 3px; }\n" % "QTableWidget#ValidatorWorkspaceConfusionTable { background: " %
             cssColor(c.surface) % "; border: 0; gridline-color: " % cssColor(c.borderSubtle) %
             "; selection-background-color: " % cssColor(c.selectionBg) % "; selection-color: " %
             cssColor(c.textDefault) % "; }\n" %
             "QTableWidget#ValidatorWorkspaceConfusionTable::item { border-bottom: 1px solid " %
             cssColor(c.borderSubtle) % "; padding: 7px 6px; }\n" %
             "QPlainTextEdit#ValidatorWorkspaceLogTextEdit { font-family: Consolas, \"Cascadia Mono\", monospace; "
             "font-size: 11px; }\n";

    style +=
        QString() % "QListWidget#ReportsWorkspaceRunList { background: " % cssColor(c.surface) %
        "; border: 0; outline: 0; }\n" % "QListWidget#ReportsWorkspaceRunList::item { border-bottom: 1px solid " %
        cssColor(c.borderSubtle) % "; padding: 10px 12px; color: " % cssColor(c.textDefault) % "; }\n" %
        "QListWidget#ReportsWorkspaceRunList::item:selected { background: " % cssColor(c.selectionBg) %
        "; border-left: 3px solid " % cssColor(c.selectionBorder) % "; }\n" %
        "QPlainTextEdit#ReportsWorkspaceSessionLogTextEdit { font-family: Consolas, \"Cascadia Mono\", monospace; "
        "font-size: 11px; }\n" %
        "QSplitter::handle, QSplitter#MainSplitter::handle, QSplitter#CameraWorkspaceSplitter::handle { background: " %
        cssColor(c.appBackground) % "; width: 8px; }\n" % "QMenu { background: " % cssColor(menuBg) % "; color: " %
        cssColor(c.textDefault) % "; border: 1px solid " % cssColor(menuBorder) % "; padding: 6px; }\n" %
        "QMenu::item { color: " % cssColor(c.textDefault) %
        "; background: transparent; padding: 7px 28px 7px 12px; min-width: 150px; }\n" %
        "QMenu::item:selected { background: " % cssColor(menuHover) % "; color: " % cssColor(c.textDefault) % "; }\n" %
        "QMenu::item:disabled { color: " % cssColor(menuDisabled) % "; background: transparent; }\n" %
        "QMenu::separator { height: 1px; background: " % cssColor(menuBorder) % "; margin: 5px 6px; }\n" %
        "QComboBox QAbstractItemView { background: " % cssColor(menuBg) % "; color: " % cssColor(c.textDefault) %
        "; border: 1px solid " % cssColor(menuBorder) % "; selection-background-color: " % cssColor(menuHover) %
        "; selection-color: " % cssColor(c.textDefault) % "; outline: 0; }\n" %
        "QComboBox QAbstractItemView::item { min-height: 24px; padding: 6px 12px; }\n" %
        "QComboBox QAbstractItemView::item:hover { background: " % cssColor(menuHover) % "; color: " %
        cssColor(c.textDefault) % "; }\n" % "QComboBox QAbstractItemView::item:selected { background: " %
        cssColor(menuHover) % "; color: " % cssColor(c.textDefault) % "; }\n" %
        "QComboBox QAbstractItemView::item:disabled { color: " % cssColor(menuDisabled) %
        "; background: transparent; }\n" % "QComboBox QAbstractItemView::item:selected:disabled { color: " %
        cssColor(menuDisabled) % "; background: transparent; }\n" %
        "QComboBox QAbstractItemView QScrollBar { background: transparent; }\n";

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

ReviewClassColors reviewClassColors(const QString& classId, ThemeMode mode) {
    return buildReviewClassColors(colors(mode), classId);
}

ReviewClassColors reviewClassColorsForBase(const QColor& baseColor, ThemeMode mode) {
    if (!baseColor.isValid())
        return reviewClassColors(QStringLiteral("0"), mode);
    return buildReviewClassColorsForBase(baseColor);
}

QString reviewClassButtonStyle(const QColor& baseColor, ThemeMode mode) {
    const auto c = colors(mode);
    const auto review = reviewClassColorsForBase(baseColor, mode);
    return QStringLiteral(
               "QPushButton { background:%1; border:1px solid %1; color:%2; font-weight:650; }"
               "QPushButton:hover { background:%3; border-color:%3; color:%2; }"
               "QPushButton:pressed { background:%4; border-color:%4; color:%2; }"
               "QPushButton:disabled { background:%5; border:1px solid %6; color:%7; }")
        .arg(cssColor(review.fill),
             cssColor(review.foreground),
             cssColor(review.hoverFill),
             cssColor(review.pressedFill),
             cssColor(c.buttonDisabledBg),
             cssColor(c.borderSubtle),
             cssColor(c.textDisabled));
}

QColor semanticClassColor(const QString& classId, ThemeMode mode) {
    return reviewClassColors(classId, mode).fill;
}

QColor semanticClassColorForBase(const QColor& baseColor, ThemeMode mode) {
    return reviewClassColorsForBase(baseColor, mode).fill;
}

QIcon semanticClassIcon(const QString& classId, ThemeMode mode) {
    const QColor fill = semanticClassColor(classId, mode);
    const QColor border = fill.darker(130);
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2.0, 2.0, 10.0, 10.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon semanticClassIconForBase(const QColor& baseColor, ThemeMode mode) {
    const QColor fill = semanticClassColorForBase(baseColor, mode);
    const QColor border = fill.darker(130);
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2.0, 2.0, 10.0, 10.0));
    painter.end();
    return QIcon(pixmap);
}
 
} // namespace desktop_app::theme
