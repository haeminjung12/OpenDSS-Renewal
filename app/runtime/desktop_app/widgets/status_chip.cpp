#include "status_chip.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QStyle>
#include <QString>

namespace {

QString stateToName(StatusChip::State state) {
    switch (state) {
    case StatusChip::State::Info:
        return QStringLiteral("info");
    case StatusChip::State::Success:
        return QStringLiteral("success");
    case StatusChip::State::Warning:
        return QStringLiteral("warning");
    case StatusChip::State::Error:
        return QStringLiteral("error");
    case StatusChip::State::Running:
        return QStringLiteral("running");
    case StatusChip::State::Armed:
        return QStringLiteral("armed");
    case StatusChip::State::Disabled:
        return QStringLiteral("disabled");
    case StatusChip::State::Neutral:
    default:
        return QStringLiteral("neutral");
    }
}

StatusChip::State nameToState(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("info")) {
        return StatusChip::State::Info;
    }
    if (normalized == QStringLiteral("success") || normalized == QStringLiteral("ok")) {
        return StatusChip::State::Success;
    }
    if (normalized == QStringLiteral("warning") || normalized == QStringLiteral("warn")) {
        return StatusChip::State::Warning;
    }
    if (normalized == QStringLiteral("error") || normalized == QStringLiteral("danger")) {
        return StatusChip::State::Error;
    }
    if (normalized == QStringLiteral("running") || normalized == QStringLiteral("active")) {
        return StatusChip::State::Running;
    }
    if (normalized == QStringLiteral("armed")) {
        return StatusChip::State::Armed;
    }
    if (normalized == QStringLiteral("disabled") || normalized == QStringLiteral("inactive")) {
        return StatusChip::State::Disabled;
    }
    return StatusChip::State::Neutral;
}

QString dotColor(StatusChip::State state) {
    switch (state) {
    case StatusChip::State::Info:
        return QStringLiteral("#3B82F6");
    case StatusChip::State::Success:
        return QStringLiteral("#22C55E");
    case StatusChip::State::Warning:
        return QStringLiteral("#F59E0B");
    case StatusChip::State::Error:
        return QStringLiteral("#EF4444");
    case StatusChip::State::Running:
        return QStringLiteral("#14B8A6");
    case StatusChip::State::Armed:
        return QStringLiteral("#A855F7");
    case StatusChip::State::Disabled:
        return QStringLiteral("#94A3B8");
    case StatusChip::State::Neutral:
    default:
        return QStringLiteral("#64748B");
    }
}

QString borderColor(StatusChip::State state) {
    switch (state) {
    case StatusChip::State::Info:
        return QStringLiteral("#3B82F6");
    case StatusChip::State::Success:
    case StatusChip::State::Running:
        return QStringLiteral("#14B8A6");
    case StatusChip::State::Warning:
        return QStringLiteral("#F59E0B");
    case StatusChip::State::Error:
        return QStringLiteral("#EF4444");
    case StatusChip::State::Armed:
        return QStringLiteral("#14B8A6");
    case StatusChip::State::Disabled:
        return QStringLiteral("#64748B");
    case StatusChip::State::Neutral:
    default:
        return QStringLiteral("#3A4352");
    }
}

} // namespace

StatusChip::StatusChip(QWidget* parent) : StatusChip(QString(), State::Neutral, parent) {}

StatusChip::StatusChip(const QString& text, State state, QWidget* parent)
    : QWidget(parent), dot_(new QFrame(this)), label_(new QLabel(this)), state_(state) {
    setObjectName(QStringLiteral("StatusChip"));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    dot_->setObjectName(QStringLiteral("StatusChipDot"));
    dot_->setFixedSize(8, 8);

    label_->setObjectName(QStringLiteral("StatusChipText"));
    label_->setText(text);
    label_->setTextFormat(Qt::PlainText);
    label_->setTextInteractionFlags(Qt::NoTextInteraction);
    label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(9, 4, 10, 4);
    layout->setSpacing(6);
    layout->addWidget(dot_);
    layout->addWidget(label_, 1);

    updateStatePresentation();
    updateAccessibleLabel();
}

QString StatusChip::text() const {
    return label_->text();
}

void StatusChip::setText(const QString& text) {
    if (label_->text() == text) {
        return;
    }
    label_->setText(text);
    updateAccessibleLabel();
}

StatusChip::State StatusChip::state() const {
    return state_;
}

void StatusChip::setState(State state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    updateStatePresentation();
    updateAccessibleLabel();
}

QString StatusChip::stateName() const {
    return stateToName(state_);
}

void StatusChip::setStateName(const QString& state) {
    setState(nameToState(state));
}

bool StatusChip::isDotVisible() const {
    return dotVisible_;
}

void StatusChip::setDotVisible(bool visible) {
    if (dotVisible_ == visible) {
        return;
    }
    dotVisible_ = visible;
    dot_->setVisible(visible);
    updateAccessibleLabel();
}

void StatusChip::updateStatePresentation() {
    const QString state = stateName();
    setProperty("state", state);
    dot_->setProperty("state", state);
    label_->setProperty("state", state);

    setStyleSheet(QStringLiteral("QWidget#StatusChip {"
                                 "background: #19202A;"
                                 "border: 1px solid %1;"
                                 "border-radius: 8px;"
                                 "color: #F1F5F9;"
                                 "}")
                      .arg(borderColor(state_)));

    dot_->setStyleSheet(QStringLiteral("QFrame#StatusChipDot {"
                                       "background: %1;"
                                       "border-radius: 4px;"
                                       "min-width: 8px;"
                                       "max-width: 8px;"
                                       "min-height: 8px;"
                                       "max-height: 8px;"
                                       "}")
                            .arg(dotColor(state_)));

    style()->unpolish(this);
    style()->polish(this);
    dot_->style()->unpolish(dot_);
    dot_->style()->polish(dot_);
    label_->style()->unpolish(label_);
    label_->style()->polish(label_);
    update();
}

void StatusChip::updateAccessibleLabel() {
    const QString label = text().isEmpty() ? QStringLiteral("Status: %1").arg(stateName())
                                           : QStringLiteral("%1 status: %2").arg(text(), stateName());
    setAccessibleName(label);
    label_->setAccessibleName(label);
}
