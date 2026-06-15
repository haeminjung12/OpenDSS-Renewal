#include "viewer_window.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

#include "app_utils.h"
#include "object_names.h"
#include "zoom_image_view.h"

ViewerWindow::ViewerWindow(QWidget* parent) : QWidget(parent) {
    nameWidget(this, "CaptureViewerWindow");
    setWindowFlags(Qt::Window);
    setWindowTitle("Capture Viewer");
    resize(1100, 800);
    setMinimumSize(800, 600);

    imageView = new ZoomImageView;
    nameWidget(imageView, "ViewerImageView");
    imageView->setMinimumSize(640, 480);
    imageView->setStyleSheet("background:#000;");
    imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    frameLabel = new QLabel("Frame: -- / --");
    timeLabel = new QLabel("Time: -- / --");
    nameWidget(frameLabel, "ViewerFrameLabel");
    nameWidget(timeLabel, "ViewerTimeLabel");
    frameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    folderEdit = new QLineEdit;
    nameWidget(folderEdit, "ViewerFolderEdit");
    folderEdit->setPlaceholderText("Select capture folder...");
    auto* browseBtn = new QPushButton("...");
    auto* loadBtn = new QPushButton("Load");
    recentCombo = new QComboBox;
    nameWidget(browseBtn, "ViewerBrowseButton");
    nameWidget(loadBtn, "ViewerLoadButton");
    nameWidget(recentCombo, "ViewerRecentComboBox");
    recentCombo->setMinimumWidth(200);

    slider = new QSlider(Qt::Horizontal);
    nameWidget(slider, "ViewerFrameSlider");
    slider->setRange(0, 0);
    slider->setEnabled(false);

    prevBtn = new QPushButton("<");
    nextBtn = new QPushButton(">");
    nameWidget(prevBtn, "ViewerPreviousFrameButton");
    nameWidget(nextBtn, "ViewerNextFrameButton");
    prevBtn->setEnabled(false);
    nextBtn->setEnabled(false);

    auto* folderRow = new QHBoxLayout;
    folderRow->addWidget(new QLabel("Folder"));
    folderRow->addWidget(folderEdit, 1);
    folderRow->addWidget(browseBtn);
    folderRow->addWidget(loadBtn);

    auto* recentRow = new QHBoxLayout;
    recentRow->addWidget(new QLabel("Recent"));
    recentRow->addWidget(recentCombo, 1);

    auto* navRow = new QHBoxLayout;
    navRow->addWidget(prevBtn);
    navRow->addWidget(nextBtn);
    navRow->addWidget(frameLabel, 1);

    auto* infoCol = new QVBoxLayout;
    infoCol->addLayout(folderRow);
    infoCol->addLayout(recentRow);
    infoCol->addWidget(timeLabel);
    infoCol->addLayout(navRow);
    infoCol->addWidget(slider);
    infoCol->addStretch(1);

    auto* rightPane = new QWidget;
    rightPane->setLayout(infoCol);
    rightPane->setMinimumWidth(320);

    auto* layout = new QHBoxLayout;
    layout->addWidget(imageView, 3);
    layout->addWidget(rightPane, 1);
    setLayout(layout);

    imageView->setZoomChanged(nullptr);

    QObject::connect(browseBtn, &QPushButton::clicked, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select capture folder", folderEdit->text());
        if (!dir.isEmpty()) {
            folderEdit->setText(dir);
        }
    });
    QObject::connect(loadBtn, &QPushButton::clicked, [this]() { loadFolder(folderEdit->text()); });
    QObject::connect(recentCombo, &QComboBox::activated, [this](int idx) {
        if (idx < 0) {
            return;
        }
        const QString dir = recentCombo->itemText(idx);
        if (!dir.isEmpty()) {
            folderEdit->setText(dir);
            loadFolder(dir);
        }
    });
    QObject::connect(slider, &QSlider::valueChanged, [this](int v) { loadFrame(v); });
    QObject::connect(prevBtn, &QPushButton::clicked, [this]() {
        if (frameFiles.isEmpty()) {
            return;
        }
        const int v = std::max(0, slider->value() - 1);
        slider->setValue(v);
    });
    QObject::connect(nextBtn, &QPushButton::clicked, [this]() {
        if (frameFiles.isEmpty()) {
            return;
        }
        const int v = std::min(slider->maximum(), slider->value() + 1);
        slider->setValue(v);
    });

    auto* leftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    auto* rightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    auto* ctrlLeftShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
    auto* ctrlRightShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this);
    auto* pageUpShortcut = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    auto* pageDownShortcut = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    QObject::connect(leftShortcut, &QShortcut::activated, [this]() { stepFrames(-1); });
    QObject::connect(rightShortcut, &QShortcut::activated, [this]() { stepFrames(1); });
    QObject::connect(ctrlLeftShortcut, &QShortcut::activated, [this]() { stepFrames(-5); });
    QObject::connect(ctrlRightShortcut, &QShortcut::activated, [this]() { stepFrames(5); });
    QObject::connect(pageUpShortcut, &QShortcut::activated, [this]() { stepFrames(-10); });
    QObject::connect(pageDownShortcut, &QShortcut::activated, [this]() { stepFrames(10); });

    loadRecentFolders();
}

void ViewerWindow::stepFrames(int delta) {
    if (frameFiles.isEmpty()) {
        return;
    }
    const int v = std::clamp(slider->value() + delta, 0, slider->maximum());
    slider->setValue(v);
}

void ViewerWindow::loadRecentFolders() {
    QSettings settings;
    const QStringList recent = settings.value("viewer/recentFolders").toStringList();
    recentCombo->clear();
    for (const QString& path : recent) {
        recentCombo->addItem(path);
    }
}

void ViewerWindow::updateRecentFolders(const QString& dirPath) {
    QSettings settings;
    QStringList recent = settings.value("viewer/recentFolders").toStringList();
    recent.removeAll(dirPath);
    recent.prepend(dirPath);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    settings.setValue("viewer/recentFolders", recent);
    recentCombo->clear();
    for (const QString& path : recent) {
        recentCombo->addItem(path);
    }
}

void ViewerWindow::loadFolder(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, "Folder not found", "The selected folder does not exist.");
        return;
    }
    QStringList filters;
    filters << "*.tif" << "*.tiff" << "*.TIF" << "*.TIFF";
    frameFiles = dir.entryList(filters, QDir::Files, QDir::Name);
    for (QString& f : frameFiles) {
        f = dir.absoluteFilePath(f);
    }
    fps = readFpsFromInfo(dir.absoluteFilePath("capture_info.txt"));
    slider->setEnabled(!frameFiles.isEmpty());
    prevBtn->setEnabled(!frameFiles.isEmpty());
    nextBtn->setEnabled(!frameFiles.isEmpty());
    const int count = static_cast<int>(frameFiles.size());
    slider->setRange(0, std::max(0, count - 1));
    slider->setValue(0);
    updateTimeLabel(0);
    if (frameFiles.isEmpty()) {
        frameLabel->setText("Frame: -- / --");
    } else {
        frameLabel->setText(QString("Frame: %1 / %2").arg(1).arg(count));
        updateRecentFolders(dir.absolutePath());
    }
}

double ViewerWindow::readFpsFromInfo(const QString& infoPath) const {
    QFile f(infoPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }
    QTextStream ts(&f);
    double foundFps = 0.0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.startsWith("Internal FPS:", Qt::CaseInsensitive) || line.startsWith("FPS:", Qt::CaseInsensitive)) {
            const QStringList parts = line.split(":");
            if (parts.size() >= 2) {
                bool ok = false;
                const double val = parts.last().trimmed().toDouble(&ok);
                if (ok) {
                    foundFps = val;
                }
            }
        }
    }
    return foundFps;
}

void ViewerWindow::loadFrame(int index) {
    if (frameFiles.isEmpty()) {
        return;
    }
    const int count = static_cast<int>(frameFiles.size());
    index = std::clamp(index, 0, count - 1);
    QImageReader reader(frameFiles.at(index));
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, "Read error", "Failed to load image:\n" + reader.errorString());
        return;
    }
    imageView->setImage(img);
    frameLabel->setText(QString("Frame: %1 / %2").arg(index + 1).arg(count));
    updateTimeLabel(index);
}

void ViewerWindow::updateTimeLabel(int index) {
    const int count = static_cast<int>(frameFiles.size());
    if (fps <= 0.0 || count == 0) {
        timeLabel->setText("Time: -- / --");
        return;
    }
    const double totalSec = static_cast<double>(count) / fps;
    const double currentSec = static_cast<double>(index) / fps;
    timeLabel->setText(QString("Time: %1 / %2").arg(formatTimeSeconds(currentSec)).arg(formatTimeSeconds(totalSec)));
}
