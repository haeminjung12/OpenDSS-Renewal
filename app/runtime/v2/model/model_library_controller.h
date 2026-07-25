#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace desktop_app::v2 {

class OperationCoordinator;
enum class ModelAccess;

class ModelLibraryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList modelRows READ modelRows NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedDetail READ selectedDetail NOTIFY changed)
    Q_PROPERTY(QString activeId READ activeId NOTIFY changed)
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(bool operationInProgress READ operationInProgress NOTIFY changed)
    Q_PROPERTY(bool canImport READ canImport NOTIFY changed)
    Q_PROPERTY(bool canExport READ canExport NOTIFY changed)
    Q_PROPERTY(bool canDuplicate READ canDuplicate NOTIFY changed)
    Q_PROPERTY(bool canDelete READ canDelete NOTIFY changed)

public:
    ModelLibraryController(QString registryFilePath,
                           OperationCoordinator &operations,
                           QObject *parent = nullptr);

    QVariantList modelRows() const;
    int selectedIndex() const;
    QString selectedId() const;
    QVariantMap selectedDetail() const;
    QString activeId() const;
    QString presentation() const;
    QString errorMessage() const;
    bool operationInProgress() const;
    bool canImport() const;
    bool canExport() const;
    bool canDuplicate() const;
    bool canDelete() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool select(int index);
    Q_INVOKABLE bool setActive();
    Q_INVOKABLE bool renameSelected(const QString &displayName);
    Q_INVOKABLE bool importModel(const QUrl &packageUrl);
    Q_INVOKABLE bool exportSelected(const QUrl &destinationRootUrl);
    Q_INVOKABLE bool duplicateSelected(const QString &displayName,
                                       const QUrl &destinationRootUrl);
    Q_INVOKABLE bool deleteSelected();

    void setActiveModelClearedCallback(std::function<void()> callback);

signals:
    void changed();

private:
    bool fail(const QString &message);
    bool selectedPackageAvailable(ModelAccess access) const;
    QString selectedPackagePath() const;
    bool validateLoadablePackage(const QString &packagePath, QString *error) const;
    bool refreshAndSelect(const QString &entryId);
    void setOperationInProgress(bool inProgress);

    OperationCoordinator &operations_;
    QString registryFilePath_;
    QJsonArray entries_;
    int selectedIndex_ = -1;
    QString errorMessage_;
    bool operationInProgress_ = false;
    std::function<void()> activeModelCleared_;
};

} // namespace desktop_app::v2
