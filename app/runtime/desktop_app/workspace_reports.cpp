#include "workspace_reports.h"

#include <QtCore>
#include <QtWidgets>

#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

QFrame* makeReportMetric(const QString& label, const QString& value, const QString& sub = QString()) {
    auto* frame = new QFrame;
    frame->setProperty("panel", true);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);
    auto* valueLabel = new QLabel(value);
    valueLabel->setProperty("metricValue", true);
    auto* labelText = new QLabel(label);
    labelText->setProperty("metricLabel", true);
    layout->addWidget(valueLabel);
    layout->addWidget(labelText);
    if (!sub.isEmpty()) {
        auto* subLabel = new QLabel(sub);
        subLabel->setProperty("mutedText", true);
        layout->addWidget(subLabel);
    }
    frame->setLayout(layout);
    return frame;
}

QJsonObject readJsonObject(const QString& path, QString* parseDiagnostic) {
    if (parseDiagnostic) parseDiagnostic->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (parseDiagnostic) {
            if (parseError.error != QJsonParseError::NoError) {
                *parseDiagnostic = QString("Malformed JSON in %1 at offset %2: %3")
                                       .arg(QFileInfo(path).fileName())
                                       .arg(parseError.offset)
                                       .arg(parseError.errorString());
            } else {
                *parseDiagnostic = QString("Top-level JSON object expected in %1.").arg(QFileInfo(path).fileName());
            }
        }
        return {};
    }
    return doc.object();
}

QString findOutputRoot() {
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 10; ++i) {
        const QString outputs = dir.filePath("outputs");
        const QString candidate = QDir(outputs).filePath("pipeline_output");
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
        if (!dir.cdUp()) break;
    }
    return {};
}

QString findLatestRunDir(const QString& outputRoot) {
    if (outputRoot.trimmed().isEmpty()) return {};
    QDir root(outputRoot);
    if (!root.exists()) return {};
    const auto candidates = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo& candidate : candidates) {
        if (!candidate.isDir()) continue;
        return candidate.absoluteFilePath();
    }
    return {};
}

}  // namespace

QWidget* buildReportsWorkspace(const ReportsWorkspaceControls& controls) {
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    auto reportsWorkspacePage = new QWidget;
    nameWidget(reportsWorkspacePage, "ReportsWorkspace");
    auto reportsWorkspaceLayout = new QHBoxLayout;
    reportsWorkspaceLayout->setContentsMargins(10, 10, 10, 10);
    reportsWorkspaceLayout->setSpacing(12);

    auto reportsRunPanel = makePanel("Reports & runs");
    reportsRunPanel->setObjectName("ReportsRunListPanel");
    reportsRunPanel->setFixedWidth(320);
    auto reportsRunBody = makePanelBody(reportsRunPanel, 0, 0, 0, 0);
    auto reportsRunList = new QListWidget;
    nameWidget(reportsRunList, "ReportsWorkspaceRunList");
    reportsRunList->setSelectionMode(QAbstractItemView::SingleSelection);
    const QVector<QStringList> reportRows = {
        {"live", "current_session", "0 events", "now"},
        {"live", "last_run_folder", "session artifacts", "recent"},
        {"validation", "validation_gui_image", "summary artifacts", "available"},
        {"training", "trainer_gui_readiness", "readiness output", "available"},
        {"logs", QFileInfo(controls.logPath).fileName(), "session stream", "active"},
    };
    for (int i = 0; i < reportRows.size(); ++i) {
        const QStringList row = reportRows.at(i);
        auto* item = new QListWidgetItem(QString("%1  %2\n%3 - %4").arg(row.at(0), row.at(1), row.at(2), row.at(3)));
        item->setData(Qt::UserRole, row.at(0));
        reportsRunList->addItem(item);
    }
    reportsRunList->setCurrentRow(0);
    reportsRunBody->addWidget(reportsRunList, 1);
    auto reportsRunHint = new QLabel("Run folders, logs, validator summaries, and trainer readiness artifacts are opened through existing actions.");
    reportsRunHint->setWordWrap(true);
    reportsRunHint->setProperty("mutedText", true);
    nameWidget(reportsRunHint, "ReportsWorkspaceRunListHint");
    reportsRunBody->addWidget(reportsRunHint);

    auto reportsRightScroll = new QScrollArea;
    nameWidget(reportsRightScroll, "ReportsWorkspaceDetailScrollArea");
    reportsRightScroll->setWidgetResizable(true);
    reportsRightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto reportsRightStack = new QWidget;
    nameWidget(reportsRightStack, "ReportsWorkspaceDetailStack");
    auto reportsRightLayout = new QVBoxLayout;
    reportsRightLayout->setContentsMargins(0, 0, 2, 0);
    reportsRightLayout->setSpacing(12);

    auto reportsSummaryPanel = makePanel("current_session", "Live sorting run - idle");
    reportsSummaryPanel->setObjectName("ReportsRunSummaryPanel");
    auto reportsSummaryBody = makePanelBody(reportsSummaryPanel);
    auto reportsMetricsRow = new QHBoxLayout;
    reportsMetricsRow->setSpacing(8);
    reportsMetricsRow->addWidget(makeReportMetric("Events", "0", "session"));
    reportsMetricsRow->addWidget(makeReportMetric("Hits", "0", "0%"));
    reportsMetricsRow->addWidget(makeReportMetric("Waste", "0", "0%"));
    reportsMetricsRow->addWidget(makeReportMetric("Duration", "00:00:00"));
    reportsSummaryBody->addLayout(reportsMetricsRow);
    auto reportsJsonDiagnostics = new QLabel("Reports JSON diagnostics: not checked");
    reportsJsonDiagnostics->setWordWrap(true);
    reportsJsonDiagnostics->setProperty("mutedText", true);
    nameWidget(reportsJsonDiagnostics, "ReportsWorkspaceJsonDiagnosticsLabel");
    reportsSummaryBody->addWidget(reportsJsonDiagnostics);
    auto reportsActionRow = new QHBoxLayout;
    reportsActionRow->setSpacing(8);
    auto reportsLogsBtn = new QPushButton("Show Logs");
    auto reportsDiagnosticsBtn = new QPushButton("Show Diagnostics");
    auto reportsRunFolderBtn = new QPushButton("Open Run Folder");
    auto reportsExportCsvBtn = new QPushButton("Export CSV");
    auto reportsOpenFiguresBtn = new QPushButton("Open Figures");
    nameWidget(reportsLogsBtn, "ReportsWorkspaceShowLogsButton");
    nameWidget(reportsDiagnosticsBtn, "ReportsWorkspaceShowDiagnosticsButton");
    nameWidget(reportsRunFolderBtn, "ReportsWorkspaceOpenRunFolderButton");
    nameWidget(reportsExportCsvBtn, "ReportsWorkspaceExportCsvButton");
    nameWidget(reportsOpenFiguresBtn, "ReportsWorkspaceOpenFiguresButton");
    reportsExportCsvBtn->setEnabled(false);
    reportsOpenFiguresBtn->setEnabled(false);
    reportsExportCsvBtn->setToolTip("CSV export remains tied to generated run artifacts; no export is synthesized here.");
    reportsOpenFiguresBtn->setToolTip("Enabled by generated report artifacts when available.");
    reportsActionRow->addWidget(reportsRunFolderBtn);
    reportsActionRow->addWidget(reportsExportCsvBtn);
    reportsActionRow->addWidget(reportsOpenFiguresBtn);
    reportsActionRow->addStretch(1);
    reportsActionRow->addWidget(reportsLogsBtn);
    reportsActionRow->addWidget(reportsDiagnosticsBtn);
    reportsSummaryBody->addLayout(reportsActionRow);
    reportsRightLayout->addWidget(reportsSummaryPanel);

    auto reportsLogPanel = makePanel("Session log", QFileInfo(controls.logPath).fileName());
    reportsLogPanel->setObjectName("ReportsSessionLogPanel");
    auto reportsLogBody = makePanelBody(reportsLogPanel);
    auto reportsLogText = new QPlainTextEdit;
    nameWidget(reportsLogText, "ReportsWorkspaceSessionLogTextEdit");
    reportsLogText->setReadOnly(true);
    reportsLogText->setMaximumHeight(280);
    QString sessionLogPreview = "Session log: " + controls.logPath + "\n\n";
    QFile sessionLogFile(controls.logPath);
    if (sessionLogFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sessionLogPreview += QString::fromUtf8(sessionLogFile.readAll()).trimmed();
    } else {
        sessionLogPreview += "Log file will stream through the existing Logs dock after runtime messages are emitted.";
    }
    reportsLogText->setPlainText(sessionLogPreview);
    reportsLogBody->addWidget(reportsLogText);
    reportsRightLayout->addWidget(reportsLogPanel);

    auto reportsDiagnosticsPanel = makePanel("Diagnostics", "Status surfaces");
    reportsDiagnosticsPanel->setObjectName("ReportsDiagnosticsPanel");
    auto reportsDiagnosticsBody = makePanelBody(reportsDiagnosticsPanel);
    auto reportsDiagnosticsRow = new QHBoxLayout;
    reportsDiagnosticsRow->setSpacing(8);
    reportsDiagnosticsRow->addWidget(makeReportMetric("Camera", controls.hardwareFreeMode ? "mock" : "startup", controls.viewerOnly ? "viewer-only" : "pending"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("Model", "not loaded", "pipeline"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("DAQ", controls.noDaq ? "disabled" : "unchecked", "trigger safe"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("Python", "not configured", "trainer/validator"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("Run", "idle", controls.hardwareFreeMode ? "no hardware" : "hardware"));
    reportsDiagnosticsBody->addLayout(reportsDiagnosticsRow);
    auto reportsDiagnosticsNote = new QLabel("Detailed hardware and subprocess diagnostics remain in the existing System Diagnostics dock.");
    reportsDiagnosticsNote->setWordWrap(true);
    reportsDiagnosticsNote->setProperty("mutedText", true);
    nameWidget(reportsDiagnosticsNote, "ReportsWorkspaceDiagnosticsNote");
    reportsDiagnosticsBody->addWidget(reportsDiagnosticsNote);
    reportsRightLayout->addWidget(reportsDiagnosticsPanel);
    reportsRightLayout->addStretch(1);
    reportsRightStack->setLayout(reportsRightLayout);
    reportsRightScroll->setWidget(reportsRightStack);
    reportsWorkspaceLayout->addWidget(reportsRunPanel, 0);
    reportsWorkspaceLayout->addWidget(reportsRightScroll, 1);
    reportsWorkspacePage->setLayout(reportsWorkspaceLayout);

    if (controls.showLogsAction) {
        QObject::connect(reportsLogsBtn, &QPushButton::clicked, controls.showLogsAction, &QAction::trigger);
    }
    if (controls.showDiagnosticsAction) {
        QObject::connect(reportsDiagnosticsBtn, &QPushButton::clicked, controls.showDiagnosticsAction, &QAction::trigger);
    }
    if (controls.openRunFolderAction) {
        QObject::connect(reportsRunFolderBtn, &QPushButton::clicked, controls.openRunFolderAction, &QAction::trigger);
    }

    const QString outputRoot = findOutputRoot();
    QString summaryDiagnostic;
    QString snapshotDiagnostic;
    QString latestRunDir;
    if (!outputRoot.isEmpty()) {
        latestRunDir = findLatestRunDir(outputRoot);
        if (!latestRunDir.isEmpty()) {
            const QString runSummaryPath = QDir(latestRunDir).filePath("run_summary.json");
            const QString snapshotPath = QDir(latestRunDir).filePath("runtime_settings_snapshot.json");
            readJsonObject(runSummaryPath, &summaryDiagnostic);
            readJsonObject(snapshotPath, &snapshotDiagnostic);
        }
    }
    if (!summaryDiagnostic.isEmpty() || !snapshotDiagnostic.isEmpty()) {
        QStringList diagnostics;
        if (!summaryDiagnostic.isEmpty()) {
            diagnostics.push_back(QString("run_summary.json: %1").arg(summaryDiagnostic));
        }
        if (!snapshotDiagnostic.isEmpty()) {
            diagnostics.push_back(QString("runtime_settings_snapshot.json: %1").arg(snapshotDiagnostic));
        }
        reportsJsonDiagnostics->setText(QString("Reports JSON diagnostics: %1").arg(diagnostics.join(" | ")));
    } else if (!latestRunDir.isEmpty()) {
        reportsJsonDiagnostics->setText("Reports JSON diagnostics: checked latest run artifacts; no malformed JSON detected.");
    } else if (!outputRoot.isEmpty()) {
        reportsJsonDiagnostics->setText("Reports JSON diagnostics: no run directory found for optional artifacts.");
    } else {
        reportsJsonDiagnostics->setText("Reports JSON diagnostics: outputs/pipeline_output not found.");
    }

    return reportsWorkspacePage;
}

}  // namespace desktop_app::workspace

