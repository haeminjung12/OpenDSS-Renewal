#pragma once

#include <QDialog>
#include <QImage>

class QLabel;
class QPushButton;

class StatsFigureWindow : public QDialog {
public:
    explicit StatsFigureWindow(QWidget* parent = nullptr);

    void setImages(const QImage& hitWaste, const QImage& classImg);
    QPushButton* saveButton() const;
    bool saveImages(const QString& dir, const QString& prefix) const;

private:
    QLabel* hitWasteLabel_ = nullptr;
    QLabel* classLabel_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QImage hitWasteImg_;
    QImage classImg_;
};
