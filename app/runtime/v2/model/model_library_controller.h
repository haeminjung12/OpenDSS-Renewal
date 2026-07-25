#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace desktop_app::v2 {

class OperationCoordinator;

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

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool select(int index);
    Q_INVOKABLE bool setActive();
    Q_INVOKABLE bool renameSelected(const QString &displayName);

signals:
    void changed();

private:
    bool fail(const QString &message);

    OperationCoordinator &operations_;
    QString registryFilePath_;
    QJsonArray entries_;
    int selectedIndex_ = -1;
    QString errorMessage_;
};

} // namespace desktop_app::v2
