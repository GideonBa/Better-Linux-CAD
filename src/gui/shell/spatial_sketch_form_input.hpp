#pragma once
#include <QAbstractSpinBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <functional>
#include <stdexcept>
namespace blcad::gui {
// Keep incomplete numeric text available for correction after focus leaves the field.
// Qt's default correction would otherwise restore/clamp it before Accept can validate it.
class SpatialFormNumberGuard final : public QObject {
public:
  SpatialFormNumberGuard(QObject& owner, QAbstractSpinBox& number, std::function<void()> changed)
      : QObject(&owner), number_(number) {
    number_.installEventFilter(this);
    if (auto* editor = number_.findChild<QLineEdit*>())
      connect(editor, &QLineEdit::textEdited, this, [changed = std::move(changed)] { changed(); });
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == &number_ && !number_.hasAcceptableInput()) {
      if (event->type() == QEvent::FocusOut)
        return true;
      if (event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Enter || key == Qt::Key_Return)
          return true;
      }
    }
    return QObject::eventFilter(watched, event);
  }

private:
  QAbstractSpinBox& number_;
};
inline void finish_spatial_number(QAbstractSpinBox& value) {
  if (!value.hasAcceptableInput()) {
    value.setFocus();
    throw std::runtime_error(
        "Complete the numeric input within its displayed range before accepting.");
  }
  value.interpretText();
}
} // namespace blcad::gui
