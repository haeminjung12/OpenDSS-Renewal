#pragma once

#include <QQuickImageProvider>

namespace desktop_app::v2::sequence {

class SequenceViewerController;

class SequenceViewerImageProvider final : public QQuickImageProvider {
  public:
    explicit SequenceViewerImageProvider(const SequenceViewerController& controller);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

  private:
    const SequenceViewerController& controller_;
};

} // namespace desktop_app::v2::sequence
