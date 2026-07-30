#pragma once

#include <QQuickImageProvider>

namespace desktop_app::v2::sequence_test {

class SequenceTestController;

class SequenceTestImageProvider final : public QQuickImageProvider {
  public:
    explicit SequenceTestImageProvider(const SequenceTestController& controller);

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

  private:
    const SequenceTestController& controller_;
};

} // namespace desktop_app::v2::sequence_test
