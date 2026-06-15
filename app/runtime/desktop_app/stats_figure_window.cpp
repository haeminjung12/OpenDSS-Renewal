#include "stats_figure_window.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "object_names.h"

StatsFigureWindow::StatsFigureWindow(QWidget* parent) : QDialog(parent) {
    nameWidget(this, "StatsFigureWindow");
    setWindowTitle("Pipeline Figures");
    resize(1100, 600);
    auto* layout = new QVBoxLayout;
    auto* row = new QHBoxLayout;
    hitWasteLabel_ = new QLabel;
    classLabel_ = new QLabel;
    nameWidget(hitWasteLabel_, "StatsHitWasteImageLabel");
    nameWidget(classLabel_, "StatsClassDistributionImageLabel");
    hitWasteLabel_->setAlignment(Qt::AlignCenter);
    classLabel_->setAlignment(Qt::AlignCenter);
    row->addWidget(hitWasteLabel_, 1);
    row->addWidget(classLabel_, 1);
    layout->addLayout(row);
    saveBtn_ = new QPushButton("Save Figures...");
    nameWidget(saveBtn_, "StatsSaveFiguresButton");
    layout->addWidget(saveBtn_, 0, Qt::AlignRight);
    setLayout(layout);
}

void StatsFigureWindow::setImages(const QImage& hitWaste, const QImage& classImg) {
    hitWasteImg_ = hitWaste;
    classImg_ = classImg;
    hitWasteLabel_->setPixmap(QPixmap::fromImage(hitWasteImg_));
    classLabel_->setPixmap(QPixmap::fromImage(classImg_));
}

QPushButton* StatsFigureWindow::saveButton() const {
    return saveBtn_;
}

bool StatsFigureWindow::saveImages(const QString& dir, const QString& prefix) const {
    if (dir.isEmpty()) {
        return false;
    }
    QDir out(dir);
    out.mkpath(".");
    const QString hitPath = out.filePath(prefix + "_hit_waste.png");
    const QString clsPath = out.filePath(prefix + "_class_dist.png");
    const bool ok1 = hitWasteImg_.isNull() ? false : hitWasteImg_.save(hitPath);
    const bool ok2 = classImg_.isNull() ? false : classImg_.save(clsPath);
    return ok1 && ok2;
}
