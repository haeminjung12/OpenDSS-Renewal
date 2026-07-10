#include "workspace_camera.h"

#include <algorithm>
#include <cmath>

#include <QtWidgets>

#include "icons.h"
#include "object_names.h"
#include "widgets/collapsible_section.h"
#include "widgets/panel_frame.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

QLabel* makeCameraFormLabel(const QString& text) {
    auto* label = desktop_app::ui::makeMutedLabel(text);
    label->setWordWrap(true);
    return label;
}

QHBoxLayout* makeCameraRow(int spacing = 8) {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(spacing);
    return row;
}

QWidget* makeCameraSegmentedControl(const QString& objectName, const QStringList& labels, int checkedIndex = 0) {
    auto* frame = new QFrame;
    nameWidget(frame, objectName.toUtf8().constData());
    frame->setProperty("panel", true);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    auto* group = new QButtonGroup(frame);
    group->setExclusive(true);

    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels.at(i));
        nameWidget(button, QString("%1Button%2").arg(objectName).arg(i).toUtf8().constData());
        button->setCheckable(true);
        button->setMinimumHeight(26);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        if (i == checkedIndex) {
            button->setChecked(true);
        }
        group->addButton(button, i);
        layout->addWidget(button);
    }

    frame->setLayout(layout);
    return frame;
}

void relaxHorizontalSize(QWidget* widget, QSizePolicy::Policy horizontalPolicy = QSizePolicy::Ignored) {
    if (!widget) {
        return;
    }
    widget->setMinimumWidth(0);
    QSizePolicy policy = widget->sizePolicy();
    policy.setHorizontalPolicy(horizontalPolicy);
    widget->setSizePolicy(policy);
}

QWidget* makeCameraFieldGroup(const QString& labelText, QWidget* field, const char* objectName = nullptr) {
    auto* group = new QWidget;
    if (objectName && *objectName) {
        nameWidget(group, objectName);
    }
    relaxHorizontalSize(group, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* label = makeCameraFormLabel(labelText);
    relaxHorizontalSize(label, QSizePolicy::Preferred);
    relaxHorizontalSize(field, QSizePolicy::Expanding);

    layout->addWidget(label);
    layout->addWidget(field);
    group->setLayout(layout);
    return group;
}

class CameraResponsiveGrid : public QWidget {
  public:
    struct Item {
        QWidget* widget = nullptr;
        int span = 1;
    };

    explicit CameraResponsiveGrid(int minimumColumnWidth, QWidget* parent = nullptr)
        : QWidget(parent), minimumColumnWidth_(minimumColumnWidth) {
        layout_ = new QGridLayout;
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setHorizontalSpacing(8);
        layout_->setVerticalSpacing(8);
        setLayout(layout_);
        relaxHorizontalSize(this, QSizePolicy::Expanding);
    }

    void addItem(QWidget* widget, int span = 1) {
        if (!widget) {
            return;
        }
        relaxHorizontalSize(widget, QSizePolicy::Expanding);
        items_.push_back({widget, span});
        relayout();
    }

  protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        relayout();
    }

  private:
    void relayout() {
        if (!layout_) {
            return;
        }

        while (layout_->count() > 0) {
            layout_->takeAt(0);
        }

        const int availableWidth = qMax(0, contentsRect().width());
        const int spacing = layout_->horizontalSpacing() > 0 ? layout_->horizontalSpacing() : 0;
        const int columns = qMax(1, (availableWidth + spacing) / qMax(1, minimumColumnWidth_ + spacing));

        int row = 0;
        int column = 0;
        for (const Item& item : items_) {
            const int span = qBound(1, item.span, columns);
            if (column + span > columns) {
                ++row;
                column = 0;
            }
            layout_->addWidget(item.widget, row, column, 1, span);
            column += span;
            if (column >= columns) {
                ++row;
                column = 0;
            }
        }

        for (int i = 0; i < columns; ++i) {
            layout_->setColumnStretch(i, 1);
        }
    }

    QGridLayout* layout_ = nullptr;
    QVector<Item> items_;
    int minimumColumnWidth_ = 160;
};

struct CameraSection {
    CollapsibleSection* section = nullptr;
    PanelFrame* panel = nullptr;
    QBoxLayout* body = nullptr;
};

CameraSection makeCameraSection(const QString& title, const char* objectName) {
    CameraSection result;
    result.section = new CollapsibleSection(title);
    nameWidget(result.section, objectName);
    result.section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    result.panel = new PanelFrame;
    result.panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    result.panel->bodyWidget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    result.panel->setObjectName(QString::fromLatin1(objectName) + "Frame");
    result.body = result.panel->bodyLayout();
    result.section->addWidget(result.panel);
    return result;
}

class CameraLutRangeBar : public QWidget {
  public:
    CameraLutRangeBar(QSlider* minSlider, QSlider* maxSlider, QWidget* parent = nullptr)
        : QWidget(parent), minSlider_(minSlider), maxSlider_(maxSlider) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(28);
        setCursor(Qt::PointingHandCursor);
        if (minSlider_) {
            QObject::connect(minSlider_, &QSlider::valueChanged, this, qOverload<>(&CameraLutRangeBar::update));
            QObject::connect(minSlider_, &QSlider::rangeChanged, this, qOverload<>(&CameraLutRangeBar::update));
        }
        if (maxSlider_) {
            QObject::connect(maxSlider_, &QSlider::valueChanged, this, qOverload<>(&CameraLutRangeBar::update));
            QObject::connect(maxSlider_, &QSlider::rangeChanged, this, qOverload<>(&CameraLutRangeBar::update));
        }
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF track = trackRect();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#2F3642"));
        painter.drawRoundedRect(track, 3.0, 3.0);

        const qreal minPos = valueToPosition(minimumValue());
        const qreal maxPos = valueToPosition(maximumValue());
        const QRectF activeTrack(QPointF(minPos, track.top()), QPointF(maxPos, track.bottom()));
        painter.setBrush(QColor("#5BC0FF"));
        painter.drawRoundedRect(activeTrack.normalized(), 3.0, 3.0);

        drawHandle(painter, minPos);
        drawHandle(painter, maxPos);
    }

    void mousePressEvent(QMouseEvent* event) override {
        activeHandle_ = chooseHandle(event->position().x());
        updateHandleFromPosition(event->position().x());
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (activeHandle_ == DragHandle::None) {
            return;
        }
        updateHandleFromPosition(event->position().x());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        activeHandle_ = DragHandle::None;
        event->accept();
    }

  private:
    enum class DragHandle { None, Min, Max };

    QRectF trackRect() const {
        constexpr qreal sidePadding = 8.0;
        constexpr qreal trackHeight = 6.0;
        const qreal top = (height() - trackHeight) * 0.5;
        return QRectF(sidePadding, top, std::max(0.0, width() - sidePadding * 2.0), trackHeight);
    }

    int rangeMinimum() const {
        return minSlider_ ? minSlider_->minimum() : 0;
    }

    int rangeMaximum() const {
        return maxSlider_ ? maxSlider_->maximum() : 0;
    }

    int minimumValue() const {
        return minSlider_ ? minSlider_->value() : 0;
    }

    int maximumValue() const {
        return maxSlider_ ? maxSlider_->value() : 0;
    }

    qreal valueToPosition(int value) const {
        const QRectF track = trackRect();
        const int minimum = rangeMinimum();
        const int maximum = std::max(minimum + 1, rangeMaximum());
        const qreal ratio = qBound(0.0, (value - minimum) / static_cast<qreal>(maximum - minimum), 1.0);
        return track.left() + ratio * track.width();
    }

    int positionToValue(qreal x) const {
        const QRectF track = trackRect();
        const int minimum = rangeMinimum();
        const int maximum = std::max(minimum, rangeMaximum());
        if (track.width() <= 0.0) {
            return minimum;
        }
        const qreal ratio = qBound(0.0, (x - track.left()) / track.width(), 1.0);
        return minimum + qRound(ratio * (maximum - minimum));
    }

    DragHandle chooseHandle(qreal x) const {
        const qreal minDistance = std::abs(x - valueToPosition(minimumValue()));
        const qreal maxDistance = std::abs(x - valueToPosition(maximumValue()));
        return minDistance <= maxDistance ? DragHandle::Min : DragHandle::Max;
    }

    void updateHandleFromPosition(qreal x) {
        if (!minSlider_ || !maxSlider_) {
            return;
        }
        const int rawValue = positionToValue(x);
        if (activeHandle_ == DragHandle::Min) {
            minSlider_->setValue(std::min(rawValue, maxSlider_->value()));
        } else if (activeHandle_ == DragHandle::Max) {
            maxSlider_->setValue(std::max(rawValue, minSlider_->value()));
        }
    }

    void drawHandle(QPainter& painter, qreal x) const {
        constexpr qreal radius = 8.0;
        const QPointF center(x, height() * 0.5);
        painter.setBrush(QColor("#F8FAFC"));
        painter.setPen(QPen(QColor("#2F3642"), 1.0));
        painter.drawEllipse(center, radius, radius);
    }

    QSlider* minSlider_ = nullptr;
    QSlider* maxSlider_ = nullptr;
    DragHandle activeHandle_ = DragHandle::None;
};

} // namespace

QString refreshCameraFormatOptions(QComboBox* presetCombo, QComboBox* bitsCombo, QComboBox* readoutCombo,
                                   QSpinBox* customWidthSpin, QSpinBox* customHeightSpin, QDoubleSpinBox* exposureSpin,
                                   const QVariantMap& options) {
    const QVariantList presets = options.value(QStringLiteral("presets")).toList();
    const QVariantList bitDepths = options.value(QStringLiteral("bitDepths")).toList();
    const QVariantList readoutSpeeds = options.value(QStringLiteral("readoutSpeeds")).toList();
    const int maximumWidth = options.value(QStringLiteral("maximumWidth")).toInt();
    const int maximumHeight = options.value(QStringLiteral("maximumHeight")).toInt();
    const double minimumExposureMs = options.value(QStringLiteral("minimumExposureMs")).toDouble();
    const double maximumExposureMs = options.value(QStringLiteral("maximumExposureMs")).toDouble();
    const double currentExposureMs = options.value(QStringLiteral("currentExposureMs")).toDouble();
    const QSize previousPreset = presetCombo->currentData().toSize();
    const QString previousPresetText = presetCombo->currentText();
    const int previousBits =
        bitsCombo->currentData().isValid() ? bitsCombo->currentData().toInt() : bitsCombo->currentText().toInt();
    const int previousReadout = readoutCombo->currentData().toInt();

    if (!presets.isEmpty()) {
        QSignalBlocker blocker(presetCombo);
        presetCombo->clear();
        for (const QVariant& presetValue : presets) {
            const QVariantMap preset = presetValue.toMap();
            const int width = preset.value(QStringLiteral("width")).toInt();
            const int height = preset.value(QStringLiteral("height")).toInt();
            const QString label = preset.value(QStringLiteral("label")).toString();
            if (width > 0 && height > 0) {
                presetCombo->addItem(label.isEmpty() ? QString("%1 x %2").arg(width).arg(height) : label,
                                     QVariant::fromValue(QSize(width, height)));
            }
        }
        presetCombo->addItem("Custom", QVariant::fromValue(QSize(-1, -1)));
        int index = presetCombo->findData(previousPreset);
        if (index < 0) {
            index = presetCombo->findText(previousPresetText);
        }
        presetCombo->setCurrentIndex(index >= 0 ? index : 0);
    }

    if (!bitDepths.isEmpty()) {
        QSignalBlocker blocker(bitsCombo);
        bitsCombo->clear();
        QString bit8Label = QStringLiteral("8");
        QString bit12Label = QStringLiteral("12");
        QString bit16Label = QStringLiteral("16");
        for (const QVariant& bitValue : bitDepths) {
            const QVariantMap bit = bitValue.toMap();
            const int value = bit.value(QStringLiteral("value")).toInt();
            const QString label = bit.value(QStringLiteral("label")).toString();
            if (value == 8 && !label.isEmpty()) {
                bit8Label = label;
            } else if (value == 12 && !label.isEmpty()) {
                bit12Label = label;
            } else if (value == 16 && !label.isEmpty()) {
                bit16Label = label;
            }
        }
        bitsCombo->addItem(bit8Label, 8);
        bitsCombo->addItem(bit12Label, 12);
        bitsCombo->addItem(bit16Label, 16);
        const int restoredBits = (previousBits == 8 || previousBits == 12 || previousBits == 16) ? previousBits : 8;
        const int index = bitsCombo->findData(restoredBits);
        bitsCombo->setCurrentIndex(index >= 0 ? index : 0);
    }

    if (!readoutSpeeds.isEmpty()) {
        QSignalBlocker blocker(readoutCombo);
        readoutCombo->clear();
        for (const QVariant& speedValue : readoutSpeeds) {
            const QVariantMap speed = speedValue.toMap();
            const int value = speed.value(QStringLiteral("value")).toInt();
            const QString label = speed.value(QStringLiteral("label")).toString();
            if (value > 0 && label.compare(QStringLiteral("Fast"), Qt::CaseInsensitive) == 0) {
                readoutCombo->addItem(label.isEmpty() ? QString::number(value) : label, value);
            }
        }
        if (readoutCombo->count() == 0) {
            readoutCombo->addItem(QStringLiteral("Fast"), 0);
        }
        const int index = readoutCombo->findData(previousReadout);
        readoutCombo->setCurrentIndex(index >= 0 ? index : 0);
        readoutCombo->setEnabled(false);
        readoutCombo->setToolTip(QStringLiteral("Readout is fixed to Fast for live camera use."));
    }

    if (maximumWidth > 0) {
        customWidthSpin->setMaximum(maximumWidth);
    }
    if (maximumHeight > 0) {
        customHeightSpin->setMaximum(maximumHeight);
    }
    if (minimumExposureMs > 0.0 && maximumExposureMs >= minimumExposureMs) {
        QSignalBlocker blocker(exposureSpin);
        exposureSpin->setMinimum(minimumExposureMs);
        exposureSpin->setMaximum(maximumExposureMs);
        if (currentExposureMs >= minimumExposureMs && currentExposureMs <= maximumExposureMs) {
            exposureSpin->setValue(currentExposureMs);
        }
    }

    const bool isCustom = presetCombo->currentData().toSize().width() < 0;
    customWidthSpin->setEnabled(isCustom);
    customHeightSpin->setEnabled(isCustom);
    return QString("Camera format options refreshed: presets=%1 bits=%2 readout=%3 max=%4x%5")
        .arg(presetCombo->count())
        .arg(bitsCombo->count())
        .arg(readoutCombo->count())
        .arg(maximumWidth)
        .arg(maximumHeight);
}

QWidget* buildCameraControlsStack(const CameraWorkspaceControls& controls) {
    auto cameraFormatSection = makeCameraSection("Format & Speed", "CameraFormatSpeedPanel");
    if (controls.presetCombo) {
        controls.presetCombo->setMinimumContentsLength(QStringLiteral("2304 x 2304").size());
    }
    if (controls.readoutCombo) {
        controls.readoutCombo->setMinimumContentsLength(QStringLiteral("Fast").size());
    }
    auto* cameraFormatGrid = new CameraResponsiveGrid(190);
    cameraFormatGrid->addItem(makeCameraFieldGroup("Preset", controls.presetCombo));
    cameraFormatGrid->addItem(makeCameraFieldGroup("Bit depth", controls.bitsCombo));
    cameraFormatGrid->addItem(makeCameraFieldGroup("Width", controls.customWidthSpin));
    cameraFormatGrid->addItem(makeCameraFieldGroup("Height", controls.customHeightSpin));
    auto* exposureRowWidget = new QWidget;
    auto* exposureRow = makeCameraRow(6);
    relaxHorizontalSize(controls.exposureSpin, QSizePolicy::Expanding);
    relaxHorizontalSize(controls.autoExposureButton, QSizePolicy::Fixed);
    exposureRow->addWidget(controls.exposureSpin, 1);
    exposureRow->addWidget(controls.autoExposureButton);
    exposureRowWidget->setLayout(exposureRow);
    cameraFormatGrid->addItem(makeCameraFieldGroup("Exposure", exposureRowWidget));
    cameraFormatGrid->addItem(makeCameraFieldGroup("Readout", controls.readoutCombo));
    cameraFormatGrid->addItem(makeCameraFieldGroup("Binning", controls.binCombo));
    cameraFormatSection.body->addWidget(cameraFormatGrid);

    auto* cameraLutGrid = new CameraResponsiveGrid(136);
    cameraLutGrid->addItem(makeCameraFieldGroup("Black", controls.lutMinSpin));
    cameraLutGrid->addItem(makeCameraFieldGroup("White", controls.lutMaxSpin));
    cameraLutGrid->addItem(makeCameraFieldGroup("Range", controls.lutAutoSetButton));
    auto* cameraLutRangeBar = new CameraLutRangeBar(controls.lutMinSlider, controls.lutMaxSlider);
    nameWidget(cameraLutRangeBar, "CameraLutRangeBar");
    relaxHorizontalSize(cameraLutRangeBar, QSizePolicy::Expanding);
    cameraLutGrid->addItem(cameraLutRangeBar, 2);
    cameraFormatSection.body->addWidget(cameraLutGrid);
    if (controls.lutRangeLabel) {
        controls.lutRangeLabel->setWordWrap(true);
        relaxHorizontalSize(controls.lutRangeLabel, QSizePolicy::Preferred);
    }
    cameraFormatSection.body->addWidget(controls.lutRangeLabel);

    auto cameraRecordingSection = makeCameraSection("Recording", "CameraRecordingPanel");
    cameraRecordingSection.body->addWidget(makeCameraFormLabel("Output folder"));
    auto* recordingPathColumn = new QVBoxLayout;
    recordingPathColumn->setContentsMargins(0, 0, 0, 0);
    recordingPathColumn->setSpacing(8);
    auto* recordingPathRow = makeCameraRow();
    relaxHorizontalSize(controls.savePathEdit);
    relaxHorizontalSize(controls.saveBrowseButton, QSizePolicy::Fixed);
    controls.saveBrowseButton->setFixedWidth(52);
    recordingPathRow->addWidget(controls.savePathEdit, 1);
    recordingPathRow->addWidget(controls.saveBrowseButton);
    recordingPathColumn->addLayout(recordingPathRow);
    relaxHorizontalSize(controls.saveOpenButton, QSizePolicy::Expanding);
    recordingPathColumn->addWidget(controls.saveOpenButton);
    cameraRecordingSection.body->addLayout(recordingPathColumn);

    auto cameraCaptureInfoCheck = new QCheckBox("Write capture_info.txt");
    nameWidget(cameraCaptureInfoCheck, "CameraWriteCaptureInfoCheckBox");
    cameraCaptureInfoCheck->setChecked(true);
    relaxHorizontalSize(cameraCaptureInfoCheck, QSizePolicy::Preferred);
    cameraRecordingSection.body->addWidget(cameraCaptureInfoCheck);
    cameraRecordingSection.body->addWidget(makeCameraFormLabel("File format"));
    cameraRecordingSection.body->addWidget(
        makeCameraSegmentedControl("CameraRecordingFormatSegmentedControl", {"TIFF", "RAW"}));
    auto* recordingButtonRow = new QGridLayout;
    recordingButtonRow->setContentsMargins(0, 0, 0, 0);
    recordingButtonRow->setHorizontalSpacing(8);
    recordingButtonRow->setVerticalSpacing(8);
    relaxHorizontalSize(controls.saveStartButton, QSizePolicy::Expanding);
    relaxHorizontalSize(controls.saveStopButton, QSizePolicy::Expanding);
    recordingButtonRow->addWidget(controls.saveStartButton, 0, 0);
    recordingButtonRow->addWidget(controls.saveStopButton, 0, 1);
    recordingButtonRow->setColumnStretch(0, 1);
    recordingButtonRow->setColumnStretch(1, 1);
    cameraRecordingSection.body->addLayout(recordingButtonRow);
    if (controls.saveInfoLabel) {
        controls.saveInfoLabel->setWordWrap(true);
        relaxHorizontalSize(controls.saveInfoLabel, QSizePolicy::Preferred);
    }
    cameraRecordingSection.body->addWidget(controls.saveInfoLabel);

    auto cameraSequenceSection = makeCameraSection("Sequence Test", "CameraSequenceTestPanel");
    if (controls.sequenceWidget) {
        relaxHorizontalSize(controls.sequenceWidget, QSizePolicy::Expanding);
        relaxHorizontalSize(controls.sequenceFolderEdit, QSizePolicy::Expanding);
        relaxHorizontalSize(controls.sequenceBrowseButton, QSizePolicy::Fixed);
        relaxHorizontalSize(controls.sequenceLoadButton, QSizePolicy::Expanding);
        relaxHorizontalSize(controls.sequenceStartButton, QSizePolicy::Expanding);
        relaxHorizontalSize(controls.sequenceStopButton, QSizePolicy::Expanding);
        relaxHorizontalSize(controls.sequenceFpsSpin, QSizePolicy::Expanding);
        if (controls.sequenceStatusLabel) {
            controls.sequenceStatusLabel->setWordWrap(true);
            relaxHorizontalSize(controls.sequenceStatusLabel, QSizePolicy::Preferred);
        }
        if (controls.sequenceLogLabel) {
            controls.sequenceLogLabel->setWordWrap(true);
            relaxHorizontalSize(controls.sequenceLogLabel, QSizePolicy::Preferred);
        }
        cameraSequenceSection.body->addWidget(controls.sequenceWidget);
    }

    auto cameraControlsStack = new QWidget;
    nameWidget(cameraControlsStack, "CameraControlsStack");
    relaxHorizontalSize(cameraControlsStack, QSizePolicy::Ignored);
    auto cameraControlsLayout = new QVBoxLayout;
    cameraControlsLayout->setContentsMargins(0, 0, 2, 0);
    cameraControlsLayout->setSpacing(12);
    cameraControlsLayout->addWidget(cameraFormatSection.section);
    cameraControlsLayout->addWidget(cameraRecordingSection.section);
    cameraControlsLayout->addWidget(cameraSequenceSection.section);
    cameraControlsLayout->addStretch(1);
    cameraControlsStack->setLayout(cameraControlsLayout);
    return cameraControlsStack;
}

} // namespace desktop_app::workspace
