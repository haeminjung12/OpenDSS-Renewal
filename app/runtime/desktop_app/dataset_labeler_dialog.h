#pragma once

#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class DatasetLabelerDialog : public QDialog {
  public:
    explicit DatasetLabelerDialog(QWidget* parent = nullptr, const QString& initialPath = QString());

  private:
    struct BrowserRow {
        int manifestIndex = -1;
        QString imageId;
        QString cropPath;
        QString autoLabel;
        QString autoSource;
        QString reviewedLabel;
        QString reviewState;
        QString eligible;
        QString warnings;
        QString confidence;
        QString sourceFramePath;
        QString notes;
        QString excludeReason;
    };

    struct ReviewUndo {
        int manifestIndex = -1;
        QJsonObject previousItem;
    };

    void loadDatasetPath(const QString& selectedPath);
    bool loadManifest(const QString& manifestPath, QStringList& report);
    bool isDatasetBuilderManifest(const QJsonObject& root) const;
    void loadBuilderManifest(const QJsonObject& root, QStringList& report);
    void loadSummaryArtifacts(QStringList& report);
    void loadClassBalanceCsv(const QString& path);
    bool loadCropsCsv(QStringList& report);
    void populateCountsFromMap(const QMap<QString, int>& counts, const QMap<QString, QString>& displayByClass);
    void populateBuilderBalanceTable(const QMap<QString, int>& autoCounts, const QMap<QString, int>& reviewedCounts,
                                     const QMap<QString, int>& eligibleCounts, int totalItems);
    void updateBannerFromBuilderCounts(const QMap<QString, int>& reviewedCounts,
                                       const QMap<QString, int>& eligibleCounts, int totalItems);
    void addLegacyBrowserRow(const QString& path, const QString& label, const QString& status, const QString& origin,
                             const QString& seed);
    bool rowMatchesFilter(const BrowserRow& row) const;
    void applyBrowserFilter();
    void updateLoadStatus();
    void setLoadStatusText(const QString& text);
    void updatePreviewFromSelection();
    QString selectedPath() const;
    void selectRelativeRow(int delta);
    void updateNavigationButtons();
    bool reviewShortcutAllowed() const;
    int selectedManifestIndex() const;
    QVector<int> selectedManifestIndexes() const;
    BrowserRow rowDataForVisibleRow(int visibleRow) const;
    BrowserRow browserRowFromBuilderItem(int manifestIndex, const QJsonObject& item) const;
    int visibleRowForManifestIndex(int manifestIndex) const;
    void updateVisibleBrowserRow(int visibleRow, const BrowserRow& rowData);
    void refreshBrowserRowFromManifestItem(int manifestIndex, const QJsonObject& item);
    void selectManifestIndexes(const QVector<int>& manifestIndexes);
    void refreshBuilderReviewSummary();
    void updateReviewControls();
    void acceptAutoLabel();
    void applyReviewLabel(const QString& label, bool advance);
    void undoLastReviewEdit();
    void rebuildRowsFromCurrentManifest();
    bool saveManifestAndLabels(bool showMessage = true);
    void writeLabelsCsv();
    QString normalizedLabel(const QString& label) const;
    QString csvEscape(QString text) const;

    QLabel* pathLabel = nullptr;
    QLabel* bannerLabel = nullptr;
    QComboBox* filterCombo = nullptr;
    QLineEdit* searchEdit = nullptr;
    QPushButton* hitButton = nullptr;
    QPushButton* wasteButton = nullptr;
    QPushButton* excludeButton = nullptr;
    QPushButton* acceptButton = nullptr;
    QPushButton* undoButton = nullptr;
    QPushButton* saveButton = nullptr;
    QComboBox* excludeReasonCombo = nullptr;
    QPlainTextEdit* notesEdit = nullptr;
    QPushButton* prevButton = nullptr;
    QPushButton* nextButton = nullptr;
    QTableWidget* browserTable = nullptr;
    QTableWidget* classBalanceTable = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* previewDetailsLabel = nullptr;
    QLabel* loadStatusLabel = nullptr;
    QLineEdit* loadStatusEdit = nullptr;
    QPlainTextEdit* outputText = nullptr;
    QVector<BrowserRow> browserRows;
    QVector<ReviewUndo> undoStack;
    QJsonDocument manifestDoc;
    QString currentDatasetPath;
    QString manifestPath;
    QString datasetRoot;
    bool isBuilderManifest = false;
};
