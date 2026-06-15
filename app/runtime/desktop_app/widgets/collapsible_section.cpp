#include "collapsible_section.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(QWidget* parent) : CollapsibleSection(QString(), parent) {}

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent), toggleButton_(new QToolButton(this)), titleLabel_(new QLabel(title, this)),
      content_(new QWidget(this)), contentLayout_(new QVBoxLayout(content_)) {
    setObjectName(QStringLiteral("CollapsibleSection"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    toggleButton_->setObjectName(QStringLiteral("CollapsibleSectionToggle"));
    toggleButton_->setAutoRaise(true);
    toggleButton_->setCheckable(true);
    toggleButton_->setChecked(true);
    toggleButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(toggleButton_, &QToolButton::clicked, this, &CollapsibleSection::toggleCollapsed);

    titleLabel_->setObjectName(QStringLiteral("CollapsibleSectionTitle"));
    titleLabel_->setTextFormat(Qt::PlainText);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(toggleButton_);
    headerLayout->addWidget(titleLabel_, 1);

    contentLayout_->setContentsMargins(0, 8, 0, 0);
    contentLayout_->setSpacing(8);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addLayout(headerLayout);
    outerLayout->addWidget(content_);

    updateHeader();
    updateAccessibleLabel();
}

QString CollapsibleSection::title() const {
    return titleLabel_->text();
}

void CollapsibleSection::setTitle(const QString& title) {
    if (titleLabel_->text() == title) {
        return;
    }
    titleLabel_->setText(title);
    updateAccessibleLabel();
}

bool CollapsibleSection::isCollapsed() const {
    return collapsed_;
}

void CollapsibleSection::setCollapsed(bool collapsed) {
    if (collapsed_ == collapsed) {
        return;
    }
    collapsed_ = collapsed;
    content_->setVisible(!collapsed_);
    updateHeader();
    updateAccessibleLabel();
    emit collapsedChanged(collapsed_);
}

QWidget* CollapsibleSection::contentWidget() const {
    return content_;
}

void CollapsibleSection::setContentWidget(QWidget* widget) {
    if (!widget || widget == content_) {
        return;
    }

    const int index = layout()->indexOf(content_);
    layout()->removeWidget(content_);
    content_->deleteLater();

    content_ = widget;
    content_->setParent(this);
    content_->setVisible(!collapsed_);
    if (index >= 0) {
        static_cast<QVBoxLayout*>(layout())->insertWidget(index, content_);
    } else {
        static_cast<QVBoxLayout*>(layout())->addWidget(content_);
    }
    contentLayout_ = qobject_cast<QBoxLayout*>(content_->layout());
}

QBoxLayout* CollapsibleSection::contentLayout() const {
    return contentLayout_;
}

void CollapsibleSection::addWidget(QWidget* widget) {
    if (!widget) {
        return;
    }
    if (!contentLayout_) {
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(0, 8, 0, 0);
        contentLayout_->setSpacing(8);
    }
    contentLayout_->addWidget(widget);
}

void CollapsibleSection::toggleCollapsed() {
    setCollapsed(!collapsed_);
}

void CollapsibleSection::updateHeader() {
    toggleButton_->setChecked(!collapsed_);
    toggleButton_->setArrowType(collapsed_ ? Qt::RightArrow : Qt::DownArrow);
    toggleButton_->setToolTip(collapsed_ ? QStringLiteral("Show section") : QStringLiteral("Hide section"));
}

void CollapsibleSection::updateAccessibleLabel() {
    const QString sectionTitle = title().isEmpty() ? QStringLiteral("Section") : title();
    setAccessibleName(QStringLiteral("%1, %2").arg(sectionTitle, collapsed_ ? QStringLiteral("collapsed")
                                                                            : QStringLiteral("expanded")));
}
