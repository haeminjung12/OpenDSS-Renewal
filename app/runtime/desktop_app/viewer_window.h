#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QSlider;

class ZoomImageView;

class ViewerWindow : public QWidget {
  public:
    explicit ViewerWindow(QWidget* parent = nullptr);

  private:
    void stepFrames(int delta);
    void loadRecentFolders();
    void updateRecentFolders(const QString& dirPath);
    void loadFolder(const QString& dirPath);
    double readFpsFromInfo(const QString& infoPath) const;
    void loadFrame(int index);
    void updateTimeLabel(int index);

    ZoomImageView* imageView = nullptr;
    QLabel* frameLabel = nullptr;
    QLabel* timeLabel = nullptr;
    QLineEdit* folderEdit = nullptr;
    QComboBox* recentCombo = nullptr;
    QSlider* slider = nullptr;
    QPushButton* prevBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QStringList frameFiles;
    double fps = 0.0;
};
