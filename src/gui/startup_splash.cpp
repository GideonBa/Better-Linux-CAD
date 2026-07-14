#include "blcad/gui/startup_splash.hpp"

#include <QFontDatabase>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <algorithm>
#include <utility>

namespace blcad::gui {
namespace {

constexpr int kSplashWidth = 780;
constexpr int kArtworkHeight = 400;
constexpr int kSplashHeight = 560;

QFont title_font() {
  QFont font;
  font.setFamilies({QStringLiteral("Times New Roman"), QStringLiteral("Tinos"),
                    QStringLiteral("Liberation Serif"), QStringLiteral("serif")});
  font.setPointSizeF(48.0);
  font.setWeight(QFont::Normal);
  font.setItalic(true);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
  return font;
}

QFont loading_font() {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(12);
  font.setBold(false);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
  return font;
}

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

  painter.fillRect(rect(), Qt::black);
  if (!artwork_.isNull()) {
    const QRect cropped_source = artwork_.rect().adjusted(12, 0, -12, 0);
    painter.drawImage(QRect(0, 0, width(), kArtworkHeight), artwork_, cropped_source);
  } else {
    painter.fillRect(QRect(0, 0, width(), kArtworkHeight), Qt::white);
  }

  QPainterPath title;
  title.addText(0.0, 0.0, title_font(), QStringLiteral("BETTER LINUX CAD"));
  QRectF title_bounds = title.boundingRect();
  if (title_bounds.width() > width() - 24) {
    const qreal scale = static_cast<qreal>(width() - 24) / title_bounds.width();
    QTransform transform;
    transform.scale(scale, scale);
    title = transform.map(title);
    title_bounds = title.boundingRect();
  }
  const qreal title_x = (width() - title_bounds.width()) / 2.0 - title_bounds.left();
  const qreal title_y = kArtworkHeight - title_bounds.top() - 1.0;
  painter.save();
  painter.translate(title_x, title_y);
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawPath(title);
  painter.restore();

  constexpr int track_left = 155;
  constexpr int track_right = 625;
  constexpr int track_y = 497;
  const int marker = track_left + (track_right - track_left) * progress_ / 100;

  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(track_left, track_y, track_right, track_y);
  painter.setPen(QPen(Qt::white, 1.0));
  painter.drawLine(marker, track_y - 7, marker, track_y + 7);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(loading_font());
  painter.setPen(Qt::white);
  painter.drawText(QRect(0, 510, width(), 32), Qt::AlignHCenter | Qt::AlignTop,
                   QStringLiteral("LOADING"));
}

} // namespace blcad::gui
