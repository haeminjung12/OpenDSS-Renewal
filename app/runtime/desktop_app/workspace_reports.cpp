#include "workspace_reports.h"

#include <QtCore>
#include <QtGui/QDesktopServices>
#include <QtWidgets>

#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

constexpr int kRunPathRole = Qt::UserRole + 1;
constexpr int kRunKindRole = Qt::UserRole + 2;
constexpr int kRunArtifactPathsRole = Qt::UserRole + 3;

struct ReportRunArtifact {
    QString kind;
    QString name;
    QString path;
    QStringList artifactPaths;
    QDateTime updated;
};

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
    if (parseDiagnostic)
        parseDiagnostic->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
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

QString configuredOutputRoot(const ReportsWorkspaceControls& controls) {
    if (controls.outputRootEdit) {
        const QString fromEdit = controls.outputRootEdit->text().trimmed();
        if (!fromEdit.isEmpty()) {
            return QDir::cleanPath(fromEdit);
        }
    }
    return QDir::cleanPath(controls.outputRoot.trimmed());
}

QString inferRunKind(const QFileInfo& info) {
    const QString name = info.fileName().toLower();
    if (name.startsWith("live_") || name.contains("_live_"))
        return "live";
    if (name.startsWith("sequence_") || name.startsWith("test_") || name.contains("sequence_"))
        return "sequence";
    if (name.contains("validation"))
        return "validation";
    if (name.contains("trainer") || name.contains("training"))
        return "training";
    return info.isDir() ? "run" : "log";
}

bool isKnownReportArtifact(const QString& fileName) {
    const QString name = fileName.toLower();
    return name == "runtime_settings_snapshot.json" || name == "run_summary.json" ||
           name.endsWith("_live_log.csv") || name.startsWith("sequence_test_log") ||
           name.startsWith("sequence_event_trajectory") || name.startsWith("sequence_summary") ||
           name.endsWith(".log") || name.endsWith(".txt");
}

QStringList artifactPathsInDirectory(const QString& dirPath, QDateTime* newest) {
    QStringList paths;
    QDir dir(dirPath);
    const auto files = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo& file : files) {
        if (!isKnownReportArtifact(file.fileName()))
            continue;
        paths.push_back(file.absoluteFilePath());
        if (newest && (!newest->isValid() || file.lastModified() > *newest)) {
            *newest = file.lastModified();
        }
    }
    return paths;
}

QVector<ReportRunArtifact> discoverRuns(const QString& rootPath) {
    QVector<ReportRunArtifact> runs;
    if (rootPath.trimmed().isEmpty())
        return runs;
    QDir root(rootPath);
    if (!root.exists())
        return runs;

    const auto dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo& dirInfo : dirs) {
        QDateTime newest = dirInfo.lastModified();
        QStringList artifacts = artifactPathsInDirectory(dirInfo.absoluteFilePath(), &newest);
        const QString kind = inferRunKind(dirInfo);
        const bool looksLikeRun = dirInfo.fileName().startsWith("live_") || dirInfo.fileName().startsWith("sequence_") ||
                                  dirInfo.fileName().startsWith("test_") || !artifacts.isEmpty();
        if (!looksLikeRun)
            continue;
        runs.push_back(ReportRunArtifact{kind, dirInfo.fileName(), dirInfo.absoluteFilePath(), artifacts, newest});
    }

    const auto files = root.entryInfoList(QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo& fileInfo : files) {
        if (!isKnownReportArtifact(fileInfo.fileName()))
            continue;
        runs.push_back(ReportRunArtifact{inferRunKind(fileInfo),
                                         fileInfo.fileName(),
                                         fileInfo.absoluteFilePath(),
                                         QStringList{fileInfo.absoluteFilePath()},
                                         fileInfo.lastModified()});
    }

    std::sort(runs.begin(), runs.end(), [](const ReportRunArtifact& a, const ReportRunArtifact& b) {
        return a.updated > b.updated;
    });
    return runs;
}

QString compactPathList(const QStringList& paths) {
    if (paths.isEmpty())
        return "No recognized report artifacts found in this run folder.";
    QStringList rows;
    for (const QString& path : paths) {
        QFileInfo info(path);
        rows.push_back(QString("%1  (%2 bytes)").arg(info.fileName()).arg(info.size()));
    }
    return rows.join("\n");
}

QString formatTimestamp(const QDateTime& timestamp) {
    if (!timestamp.isValid())
        return "unknown";
    return timestamp.toString("yyyy-MM-dd HH:mm:ss");
}

void openExistingPath(const QString& path) {
    if (path.trimmed().isEmpty())
        return;
    const QFileInfo info(path);
    if (!info.exists())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
}

} // namespace

QWidget* buildReportsWorkspace(const ReportsWorkspaceControls& controls) {
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    auto* reportsWorkspacePage = new QWidget;
    nameWidget(reportsWorkspacePage, "ReportsWorkspace");
    auto* reportsWorkspaceLayout = new QHBoxLayout;
    reportsWorkspaceLayout->setContentsMargins(10, 10, 10, 10);
    reportsWorkspaceLayout->setSpacing(12);

    auto* reportsRunPanel = makePanel("Reports & runs");
    reportsRunPanel->setObjectName("ReportsRunListPanel");
    reportsRunPanel->setFixedWidth(320);
    auto* reportsRunBody = makePanelBody(reportsRunPanel, 0, 0, 0, 0);
    auto* reportsRunList = new QListWidget;
    nameWidget(reportsRunList, "ReportsWorkspaceRunList");
    reportsRunList->setSelectionMode(QAbstractItemView::SingleSelection);
    reportsRunBody->addWidget(reportsRunList, 1);

    auto* reportsRunHint = new QLabel;
    reportsRunHint->setWordWrap(true);
    reportsRunHint->setProperty("mutedText", true);
    nameWidget(reportsRunHint, "ReportsWorkspaceRunListHint");
    reportsRunBody->addWidget(reportsRunHint);

    auto* reportsRightScroll = new QScrollArea;
    nameWidget(reportsRightScroll, "ReportsWorkspaceDetailScrollArea");
    reportsRightScroll->setWidgetResizable(true);
    reportsRightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* reportsRightStack = new QWidget;
    nameWidget(reportsRightStack, "ReportsWorkspaceDetailStack");
    auto* reportsRightLayout = new QVBoxLayout;
    reportsRightLayout->setContentsMargins(0, 0, 2, 0);
    reportsRightLayout->setSpacing(12);

    auto* reportsSummaryPanel = makePanel("Selected run", "Existing runtime artifacts");
    reportsSummaryPanel->setObjectName("ReportsRunSummaryPanel");
    auto* reportsSummaryBody = makePanelBody(reportsSummaryPanel);
    auto* reportsMetricsRow = new QHBoxLayout;
    reportsMetricsRow->setSpacing(8);
    auto* reportsTypeMetric = makeReportMetric("Type", "none", "selected");
    auto* reportsArtifactMetric = makeReportMetric("Artifacts", "0", "recognized");
    auto* reportsUpdatedMetric = makeReportMetric("Updated", "unknown", "local time");
    auto* reportsRootMetric = makeReportMetric("Root", "not found", "configured output");
    reportsMetricsRow->addWidget(reportsTypeMetric);
    reportsMetricsRow->addWidget(reportsArtifactMetric);
    reportsMetricsRow->addWidget(reportsUpdatedMetric);
    reportsMetricsRow->addWidget(reportsRootMetric);
    reportsSummaryBody->addLayout(reportsMetricsRow);

    auto* reportsSelectedName = new QLabel("No run selected");
    reportsSelectedName->setProperty("metricValue", true);
    reportsSelectedName->setWordWrap(true);
    nameWidget(reportsSelectedName, "ReportsWorkspaceSelectedRunNameLabel");
    reportsSummaryBody->addWidget(reportsSelectedName);

    auto* reportsSelectedPath = new QLabel;
    reportsSelectedPath->setWordWrap(true);
    reportsSelectedPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    reportsSelectedPath->setProperty("mutedText", true);
    nameWidget(reportsSelectedPath, "ReportsWorkspaceSelectedRunPathLabel");
    reportsSummaryBody->addWidget(reportsSelectedPath);

    auto* reportsArtifactText = new QPlainTextEdit;
    reportsArtifactText->setReadOnly(true);
    reportsArtifactText->setMaximumHeight(150);
    nameWidget(reportsArtifactText, "ReportsWorkspaceArtifactTextEdit");
    reportsSummaryBody->addWidget(reportsArtifactText);

    auto* reportsJsonDiagnostics = new QLabel("Reports JSON diagnostics: not checked");
    reportsJsonDiagnostics->setWordWrap(true);
    reportsJsonDiagnostics->setProperty("mutedText", true);
    nameWidget(reportsJsonDiagnostics, "ReportsWorkspaceJsonDiagnosticsLabel");
    reportsSummaryBody->addWidget(reportsJsonDiagnostics);

    auto* reportsActionRow = new QHBoxLayout;
    reportsActionRow->setSpacing(8);
    auto* reportsLogsBtn = new QPushButton("Show Logs");
    auto* reportsDiagnosticsBtn = new QPushButton("Show Diagnostics");
    auto* reportsRunFolderBtn = new QPushButton("Open Run Folder");
    auto* reportsOpenOutputBtn = new QPushButton("Open Output Root");
    auto* reportsExportCsvBtn = new QPushButton("Export CSV");
    auto* reportsOpenFiguresBtn = new QPushButton("Open Figures");
    nameWidget(reportsLogsBtn, "ReportsWorkspaceShowLogsButton");
    nameWidget(reportsDiagnosticsBtn, "ReportsWorkspaceShowDiagnosticsButton");
    nameWidget(reportsRunFolderBtn, "ReportsWorkspaceOpenRunFolderButton");
    nameWidget(reportsOpenOutputBtn, "ReportsWorkspaceOpenOutputButton");
    nameWidget(reportsExportCsvBtn, "ReportsWorkspaceExportCsvButton");
    nameWidget(reportsOpenFiguresBtn, "ReportsWorkspaceOpenFiguresButton");
    reportsExportCsvBtn->setEnabled(false);
    reportsOpenFiguresBtn->setEnabled(false);
    reportsExportCsvBtn->setToolTip(
        "CSV export remains tied to generated run artifacts; no export is synthesized here.");
    reportsOpenFiguresBtn->setToolTip("Enabled by generated report artifacts when available.");
    reportsActionRow->addWidget(reportsRunFolderBtn);
    reportsActionRow->addWidget(reportsOpenOutputBtn);
    reportsActionRow->addWidget(reportsExportCsvBtn);
    reportsActionRow->addWidget(reportsOpenFiguresBtn);
    reportsActionRow->addStretch(1);
    reportsActionRow->addWidget(reportsLogsBtn);
    reportsActionRow->addWidget(reportsDiagnosticsBtn);
    reportsSummaryBody->addLayout(reportsActionRow);
    reportsRightLayout->addWidget(reportsSummaryPanel);

    auto* reportsLogPanel = makePanel("Session log", QFileInfo(controls.logPath).fileName());
    reportsLogPanel->setObjectName("ReportsSessionLogPanel");
    auto* reportsLogBody = makePanelBody(reportsLogPanel);
    auto* reportsLogText = new QPlainTextEdit;
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

    auto* reportsDiagnosticsPanel = makePanel("Diagnostics", "Status surfaces");
    reportsDiagnosticsPanel->setObjectName("ReportsDiagnosticsPanel");
    auto* reportsDiagnosticsBody = makePanelBody(reportsDiagnosticsPanel);
    auto* reportsDiagnosticsRow = new QHBoxLayout;
    reportsDiagnosticsRow->setSpacing(8);
    reportsDiagnosticsRow->addWidget(makeReportMetric("Camera", controls.hardwareFreeMode ? "mock" : "startup",
                                                      controls.viewerOnly ? "viewer-only" : "pending"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("Model", "not loaded", "pipeline"));
    reportsDiagnosticsRow->addWidget(
        makeReportMetric("DAQ", controls.noDaq ? "disabled" : "unchecked", "trigger safe"));
    reportsDiagnosticsRow->addWidget(makeReportMetric("Python", "not configured", "trainer/validator"));
    reportsDiagnosticsRow->addWidget(
        makeReportMetric("Run", "idle", controls.hardwareFreeMode ? "no hardware" : "hardware"));
    reportsDiagnosticsBody->addLayout(reportsDiagnosticsRow);
    auto* reportsDiagnosticsNote =
        new QLabel("Detailed hardware and subprocess diagnostics remain in the existing System Diagnostics dock.");
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

    auto metricValueLabel = [](QFrame* frame) -> QLabel* {
        const auto labels = frame->findChildren<QLabel*>();
        return labels.isEmpty() ? nullptr : labels.first();
    };
    auto* typeValue = metricValueLabel(reportsTypeMetric);
    auto* artifactValue = metricValueLabel(reportsArtifactMetric);
    auto* updatedValue = metricValueLabel(reportsUpdatedMetric);
    auto* rootValue = metricValueLabel(reportsRootMetric);

    const QString outputRoot = configuredOutputRoot(controls);
    const QVector<ReportRunArtifact> discoveredRuns = discoverRuns(outputRoot);
    const bool outputRootExists = !outputRoot.isEmpty() && QFileInfo(outputRoot).isDir();
    if (rootValue) {
        rootValue->setText(outputRootExists ? QFileInfo(outputRoot).fileName() : "not found");
    }
    reportsOpenOutputBtn->setEnabled(outputRootExists);

    for (const ReportRunArtifact& run : discoveredRuns) {
        const QString artifactSummary =
            run.artifactPaths.isEmpty() ? "folder" : QString("%1 artifacts").arg(run.artifactPaths.size());
        auto* item = new QListWidgetItem(
            QString("%1  %2\n%3 - %4").arg(run.kind, run.name, artifactSummary, formatTimestamp(run.updated)));
        item->setData(kRunPathRole, run.path);
        item->setData(kRunKindRole, run.kind);
        item->setData(kRunArtifactPathsRole, run.artifactPaths);
        reportsRunList->addItem(item);
    }

    reportsRunHint->setText(
        outputRootExists
            ? QString("Output root: %1").arg(outputRoot)
            : QString("No output root found. Configure a run output folder in Settings before generating reports."));

    auto updateSelectedRun = [=]() {
        const QListWidgetItem* item = reportsRunList->currentItem();
        const bool hasSelection = item != nullptr;
        const QString selectedPath = hasSelection ? item->data(kRunPathRole).toString() : QString();
        const QFileInfo selectedInfo(selectedPath);
        const QStringList artifacts = hasSelection ? item->data(kRunArtifactPathsRole).toStringList() : QStringList();
        reportsRunFolderBtn->setEnabled(hasSelection && selectedInfo.exists());
        if (typeValue)
            typeValue->setText(hasSelection ? item->data(kRunKindRole).toString() : "none");
        if (artifactValue)
            artifactValue->setText(QString::number(artifacts.size()));
        if (updatedValue)
            updatedValue->setText(hasSelection ? formatTimestamp(selectedInfo.lastModified()) : "unknown");
        reportsSelectedName->setText(hasSelection ? selectedInfo.fileName() : "No run selected");
        reportsSelectedPath->setText(hasSelection ? selectedInfo.absoluteFilePath()
                                                  : "No run artifacts were found in the configured output root.");
        reportsArtifactText->setPlainText(hasSelection ? compactPathList(artifacts)
                                                       : "Start a live or sequence run to create report artifacts.");

        QStringList diagnostics;
        if (hasSelection && selectedInfo.isDir()) {
            const QString runSummaryPath = QDir(selectedInfo.absoluteFilePath()).filePath("run_summary.json");
            const QString snapshotPath =
                QDir(selectedInfo.absoluteFilePath()).filePath("runtime_settings_snapshot.json");
            QString summaryDiagnostic;
            QString snapshotDiagnostic;
            readJsonObject(runSummaryPath, &summaryDiagnostic);
            readJsonObject(snapshotPath, &snapshotDiagnostic);
            if (!summaryDiagnostic.isEmpty())
                diagnostics.push_back(QString("run_summary.json: %1").arg(summaryDiagnostic));
            if (!snapshotDiagnostic.isEmpty())
                diagnostics.push_back(QString("runtime_settings_snapshot.json: %1").arg(snapshotDiagnostic));
            if (diagnostics.isEmpty()) {
                diagnostics.push_back("checked selected run artifacts; no malformed JSON detected");
            }
        } else if (hasSelection) {
            diagnostics.push_back("selected artifact is a file; no run JSON diagnostics available");
        } else if (outputRootExists) {
            diagnostics.push_back("no run folders or report log files found");
        } else {
            diagnostics.push_back("configured output root is missing");
        }
        reportsJsonDiagnostics->setText(QString("Reports JSON diagnostics: %1.").arg(diagnostics.join(" | ")));
    };

    QObject::connect(reportsRunList, &QListWidget::currentItemChanged, reportsWorkspacePage,
                     [updateSelectedRun](QListWidgetItem*, QListWidgetItem*) { updateSelectedRun(); });
    QObject::connect(reportsRunFolderBtn, &QPushButton::clicked, reportsWorkspacePage, [reportsRunList]() {
        const QListWidgetItem* item = reportsRunList->currentItem();
        if (!item)
            return;
        openExistingPath(item->data(kRunPathRole).toString());
    });
    QObject::connect(reportsOpenOutputBtn, &QPushButton::clicked, reportsWorkspacePage,
                     [outputRoot]() { openExistingPath(outputRoot); });

    if (controls.showLogsAction) {
        QObject::connect(reportsLogsBtn, &QPushButton::clicked, controls.showLogsAction, &QAction::trigger);
    }
    if (controls.showDiagnosticsAction) {
        QObject::connect(reportsDiagnosticsBtn, &QPushButton::clicked, controls.showDiagnosticsAction,
                         &QAction::trigger);
    }

    if (reportsRunList->count() > 0) {
        reportsRunList->setCurrentRow(0);
    } else {
        updateSelectedRun();
    }

    return reportsWorkspacePage;
}

} // namespace desktop_app::workspace
