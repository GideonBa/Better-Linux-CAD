#include "blcad/gui/startup_splash.hpp"
#include "blcad/gui/gui_appearance.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QVariantAnimation>

#include <algorithm>
#include <utility>

namespace blcad::gui {
namespace {

constexpr int kSplashWidth = 780;
constexpr int kArtworkHeight = 400;
constexpr int kSplashHeight = 560;

} // namespace

StartupSplash::StartupSplash(QWidget* parent)
    : QWidget(parent), artwork_(QStringLiteral(":/blcad/gui/archimedes-lever.gif")),
      animation_(new QVariantAnimation(this)) {
  setObjectName(QStringLiteral("blcad.startup_splash"));
  setAccessibleName(QStringLiteral("Better Linux CAD loading screen"));
  setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setFixedSize(sizeHint());

  connect(animation_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) { set_progress(value.toInt()); });
  connect(animation_, &QVariantAnimation::finished, this, [this] {
    set_progress(100);
    auto callback = std::exchange(finished_, {});
    if (callback)
      callback();
  });
}

void StartupSplash::set_progress(int progress) {
  const int bounded = std::clamp(progress, 0, 100);
  if (bounded == progress_)
    return;
  progress_ = bounded;
  update();
}

void StartupSplash::start(int duration_ms, std::function<void()> finished) {
  animation_->stop();
  finished_ = std::move(finished);
  animation_->setStartValue(progress_);
  animation_->setEndValue(100);
  animation_->setDuration(std::max(duration_ms, 1));
  animation_->setEasingCurve(QEasingCurve::InOutCubic);
  animation_->start();
}

int StartupSplash::progress() const noexcept {
  return progress_;
}

bool StartupSplash::has_artwork() const noexcept {
  return !artwork_.isNull();
}

QSize StartupSplash::sizeHint() const {
  return {kSplashWidth, kSplashHeight};
}

void StartupSplash::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const QColor blue = QColor::fromRgb(appearance::blue);
  const QColor paper = QColor::fromRgb(appearance::paper);
  painter.fillRect(rect(), paper);
  if (!artwork_.isNull()) {
    const QRect cropped_source = artwork_.rect().adjusted(12, 0, -12, 0);
    painter.drawImage(QRect(0, 0, width(), kArtworkHeight), artwork_, cropped_source);
  }

  painter.fillRect(QRect(0, kArtworkHeight, width(), height() - kArtworkHeight), blue);
  painter.setPen(paper);
  painter.setFont(appearance::heading_font(64));
  painter.drawText(QRect(30, 405, width() - 60, 74), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("BETTER LINUX CAD"));

  painter.setFont(appearance::numeric_font(11));
  painter.drawText(QRect(32, 479, width() - 64, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("PARAMETRIC DESIGN / ENGINEERING WORKSPACE"));

  constexpr int track_left = 32;
  constexpr int track_right = kSplashWidth - 32;
  constexpr int track_y = 518;
  const int marker = track_left + (track_right - track_left) * progress_ / 100;

  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(paper, 1.0, Qt::SolidLine, Qt::SquareCap));
  painter.drawLine(track_left, track_y, track_right, track_y);
  painter.fillRect(QRect(track_left, track_y - 2, marker - track_left, 5), paper);
  painter.setPen(QPen(blue, 1.0));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(appearance::numeric_font(12));
  painter.setPen(paper);
  painter.drawText(QRect(32, 531, width() - 64, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("INITIALIZING WORKSPACE"));
  painter.drawText(QRect(32, 531, width() - 64, 22), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1%").arg(progress_));
}

} // namespace blcad::gui
