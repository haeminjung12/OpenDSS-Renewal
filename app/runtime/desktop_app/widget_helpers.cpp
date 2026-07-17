#include "widget_helpers.h"

#include "object_names.h"

#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QToolButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace desktop_app::ui {
namespace {

constexpr auto kWorkspaceSplitterSettingsKeyProperty = "_ovdsWorkspaceSplitterSettingsKey";
constexpr auto kWorkspaceSplitterDefaultSizesProperty = "_ovdsWorkspaceSplitterDefaultSizes";

QVariantList toVariantList(const QList<int>& values) {
    QVariantList result;
    result.reserve(values.size());
    for (const int value : values)
        result.push_back(value);
    return result;
}

QList<int> fromVariantList(const QVariant& value) {
    QList<int> result;
    for (const QVariant& item : value.toList())
        result.push_back(item.toInt());
    return result;
}

void applySectionMinimums(QSplitter* splitter, const QList<int>& minimumSectionSizes, const Qt::Orientation orientation) {
    if (!splitter)
        return;
    for (int index = 0; index < splitter->count(); ++index) {
        splitter->setCollapsible(index, false);
        if (index >= minimumSectionSizes.size())
            continue;
        if (QWidget* section = splitter->widget(index)) {
            if (orientation == Qt::Horizontal)
                section->setMinimumWidth(minimumSectionSizes.at(index));
            else
                section->setMinimumHeight(minimumSectionSizes.at(index));
        }
    }
}

} // namespace

QFrame* makePanel(const QString& title, const QString& subtitle) {
    auto* panel = new QFrame;
    panel->setProperty("panel", true);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QWidget;
    auto* headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(12, 9, 12, 9);
    headerLayout->setSpacing(8);
    auto* titleLabel = new QLabel(title);
    titleLabel->setProperty("panelTitle", true);
    headerLayout->addWidget(titleLabel);
    if (!subtitle.isEmpty()) {
        auto* subtitleLabel = new QLabel(subtitle);
        subtitleLabel->setProperty("panelSubtitle", true);
        headerLayout->addWidget(subtitleLabel);
    }
    headerLayout->addStretch(1);
    header->setLayout(headerLayout);
    layout->addWidget(header);
    panel->setLayout(layout);
    return panel;
}

QVBoxLayout* makePanelBody(QFrame* panel, int left, int top, int right, int bottom) {
    auto* body = new QWidget;
    auto* bodyLayout = new QVBoxLayout;
    bodyLayout->setContentsMargins(left, top, right, bottom);
    bodyLayout->setSpacing(10);
    body->setLayout(bodyLayout);
    qobject_cast<QVBoxLayout*>(panel->layout())->addWidget(body, 1);
    return bodyLayout;
}

QFrame* makeMetric(const QString& label, QLabel* valueLabel) {
    auto* cell = new QFrame;
    cell->setProperty("panel", true);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);
    valueLabel->setProperty("metricValue", true);
    auto* labelWidget = new QLabel(label);
    labelWidget->setProperty("metricLabel", true);
    layout->addWidget(labelWidget);
    layout->addWidget(valueLabel);
    cell->setLayout(layout);
    return cell;
}

QWidget* makeStatusRow(const QString& label, const QString& value, const QString& tone) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* dot = new QLabel("o");
    dot->setProperty("statusDot", true);
    if (!tone.trimmed().isEmpty())
        dot->setProperty("statusTone", tone.trimmed());
    auto* labelWidget = new QLabel(label);
    labelWidget->setProperty("mutedText", true);
    auto* valueWidget = new QLabel(value);
    valueWidget->setProperty("statusPill", true);
    valueWidget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    valueWidget->setMinimumWidth(72);
    layout->addWidget(dot);
    layout->addWidget(labelWidget, 1);
    layout->addWidget(valueWidget, 0, Qt::AlignRight);
    row->setLayout(layout);
    return row;
}

QPushButton* makeToolButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setMinimumHeight(24);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

QFrame* makeCollapsedGroup(const QString& title, QWidget* child) {
    auto* section = new QFrame;
    section->setFrameShape(QFrame::StyledPanel);
    section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* toggle = new QToolButton;
    toggle->setText(title);
    toggle->setCheckable(true);
    toggle->setChecked(false);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toggle->setAccessibleName(title);

    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(8, 6, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(toggle);
    layout->addWidget(child);
    section->setLayout(layout);

    child->setVisible(false);
    QObject::connect(toggle, &QToolButton::toggled, section, [toggle, child, section](bool checked) {
        toggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        child->setVisible(checked);
        child->updateGeometry();
        section->updateGeometry();
    });
    return section;
}

QWidget* makeWorkspacePlaceholder(const QString& title, const QString& body, const char* objectName) {
    auto* page = new QWidget;
    nameWidget(page, objectName);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QString::fromLatin1(objectName) + "Title");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    auto* bodyLabel = new QLabel(body);
    bodyLabel->setWordWrap(true);
    bodyLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);
    layout->addStretch(1);
    page->setLayout(layout);
    return page;
}

QLabel* makeMutedLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setProperty("mutedText", true);
    return label;
}

void configureWorkspaceSplitter(QSplitter* splitter,
                                const QString& settingsKey,
                                const QList<int>& defaultSizes,
                                const QList<int>& minimumSectionSizes,
                                const Qt::Orientation orientation) {
    if (!splitter)
        return;
    splitter->setChildrenCollapsible(false);
    splitter->setOpaqueResize(true);
    splitter->setProperty(kWorkspaceSplitterSettingsKeyProperty, settingsKey);
    splitter->setProperty(kWorkspaceSplitterDefaultSizesProperty, toVariantList(defaultSizes));
    applySectionMinimums(splitter, minimumSectionSizes, orientation);
    QObject::connect(splitter, &QSplitter::splitterMoved, splitter, [splitter]() { saveWorkspaceSplitterState(splitter); });
    restoreWorkspaceSplitterState(splitter);
    QTimer::singleShot(0, splitter, [splitter]() { restoreWorkspaceSplitterState(splitter); });
}

bool restoreWorkspaceSplitterState(QSplitter* splitter) {
    if (!splitter)
        return false;
    const QString settingsKey = splitter->property(kWorkspaceSplitterSettingsKeyProperty).toString().trimmed();
    const QList<int> defaultSizes = fromVariantList(splitter->property(kWorkspaceSplitterDefaultSizesProperty));
    bool restored = false;
    if (!settingsKey.isEmpty()) {
        QSettings settings;
        const QByteArray state = settings.value(settingsKey).toByteArray();
        if (!state.isEmpty())
            restored = splitter->restoreState(state);
    }
    if (!restored && !defaultSizes.isEmpty())
        splitter->setSizes(defaultSizes);
    return restored;
}

void saveWorkspaceSplitterState(QSplitter* splitter) {
    if (!splitter)
        return;
    const QString settingsKey = splitter->property(kWorkspaceSplitterSettingsKeyProperty).toString().trimmed();
    if (settingsKey.isEmpty())
        return;
    QSettings settings;
    settings.setValue(settingsKey, splitter->saveState());
    settings.sync();
}

} // namespace desktop_app::ui
