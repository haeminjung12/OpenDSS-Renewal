#include "metric_tile.h"

#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

MetricTile::MetricTile(QWidget* parent) : MetricTile(QString(), QStringLiteral("--"), QString(), parent) {}

MetricTile::MetricTile(const QString& label, const QString& value, const QString& sublabel, QWidget* parent)
    : QWidget(parent), label_(new QLabel(label, this)), value_(new QLabel(value, this)),
      sublabel_(new QLabel(sublabel, this)) {
    setObjectName(QStringLiteral("MetricTile"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    label_->setObjectName(QStringLiteral("MetricTileLabel"));
    label_->setTextFormat(Qt::PlainText);

    value_->setObjectName(QStringLiteral("MetricTileValue"));
    value_->setTextFormat(Qt::PlainText);
    value_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    sublabel_->setObjectName(QStringLiteral("MetricTileSublabel"));
    sublabel_->setTextFormat(Qt::PlainText);
    sublabel_->setVisible(!sublabel.isEmpty());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(2);
    layout->addWidget(label_);
    layout->addWidget(value_);
    layout->addWidget(sublabel_);

    updateAccessibleLabel();
}

QString MetricTile::label() const {
    return label_->text();
}

void MetricTile::setLabel(const QString& label) {
    if (label_->text() == label) {
        return;
    }
    label_->setText(label);
    updateAccessibleLabel();
}

QString MetricTile::value() const {
    return value_->text();
}

void MetricTile::setValue(const QString& value) {
    if (value_->text() == value) {
        return;
    }
    value_->setText(value);
    updateAccessibleLabel();
}

QString MetricTile::sublabel() const {
    return sublabel_->text();
}

void MetricTile::setSublabel(const QString& sublabel) {
    if (sublabel_->text() == sublabel) {
        return;
    }
    sublabel_->setText(sublabel);
    sublabel_->setVisible(!sublabel.isEmpty());
    updateAccessibleLabel();
}

void MetricTile::updateAccessibleLabel() {
    QString accessible = label().isEmpty() ? value() : QStringLiteral("%1: %2").arg(label(), value());
    if (!sublabel().isEmpty()) {
        accessible += QStringLiteral(", %1").arg(sublabel());
    }
    setAccessibleName(accessible);
}
