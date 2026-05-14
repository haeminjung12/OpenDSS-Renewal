#include "panel_frame.h"

#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

PanelFrame::PanelFrame(QWidget* parent)
    : PanelFrame(QString(), parent) {}

PanelFrame::PanelFrame(const QString& title, QWidget* parent)
    : QFrame(parent),
      titleLabel_(new QLabel(title, this)),
      body_(new QWidget(this)),
      bodyLayout_(new QVBoxLayout(body_)) {
    setObjectName(QStringLiteral("PanelFrame"));
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    titleLabel_->setObjectName(QStringLiteral("PanelFrameTitle"));
    titleLabel_->setTextFormat(Qt::PlainText);

    body_->setObjectName(QStringLiteral("PanelFrameBody"));
    bodyLayout_->setContentsMargins(0, 0, 0, 0);
    bodyLayout_->setSpacing(8);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    layout->addWidget(titleLabel_);
    layout->addWidget(body_);

    updateTitleVisibility();
}

QString PanelFrame::title() const {
    return titleLabel_->text();
}

void PanelFrame::setTitle(const QString& title) {
    if (titleLabel_->text() == title) {
        return;
    }
    titleLabel_->setText(title);
    updateTitleVisibility();
    setAccessibleName(title);
}

QWidget* PanelFrame::bodyWidget() const {
    return body_;
}

QBoxLayout* PanelFrame::bodyLayout() const {
    return bodyLayout_;
}

void PanelFrame::addWidget(QWidget* widget) {
    if (!widget) {
        return;
    }
    bodyLayout_->addWidget(widget);
}

void PanelFrame::updateTitleVisibility() {
    titleLabel_->setVisible(!titleLabel_->text().isEmpty());
}
