#pragma once

#include <QImage>
#include <QWidget>

#include <functional>

class QVariantAnimation;

namespace blcad::gui {

class StartupSplash final : public QWidget {
public:
  explicit StartupSplash(QWidget* parent = nullptr);

  void set_progress(int progress);
  void start(int duration_ms, std::function<void()> finished);

  [[nodiscard]] int progress() const noexcept;
  [[nodiscard]] bool has_artwork() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QImage artwork_;
  QVariantAnimation* animation_{nullptr};
  std::function<void()> finished_;
  int progress_{0};
};

} // namespace blcad::gui
