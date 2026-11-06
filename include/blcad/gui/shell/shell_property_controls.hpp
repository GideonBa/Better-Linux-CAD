#pragma once

#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QToolButton;
class QVBoxLayout;

namespace blcad::gui {

// Transient presentation only. Commands keep their candidate, references and
// transaction authority; the group never edits a document or owns a task.
class ShellPropertyGroup final : public QWidget {
public:
  ShellPropertyGroup(QString id, QString title, QWidget* parent = nullptr);
  [[nodiscard]] QWidget& content() const noexcept;
  [[nodiscard]] QVBoxLayout& content_layout() const noexcept;
  [[nodiscard]] bool expanded() const noexcept;
  void set_expanded(bool expanded);
  void set_diagnostic(QString text);

private:
  void refresh_heading();
  QString title_;
  QToolButton* heading_;
  QWidget* content_;
  QVBoxLayout* content_layout_;
  QLabel* diagnostic_;
};

// The existing combo retains its model and semantic data. Pick delegates to the
// explicitly supplied owner; changing presentation cannot invent a target.
class ShellPropertySelector final : public QWidget {
public:
  ShellPropertySelector(QString id, QString title, QComboBox& choices, QObject& owner,
                        std::function<void(bool)> pick, QWidget* parent = nullptr);
  void set_active(bool active);
  void set_diagnostic(QString text);
  [[nodiscard]] bool active() const noexcept;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void refresh_count();
  QComboBox& choices_;
  QToolButton* pick_;
  QToolButton* clear_;
  QLabel* count_;
  QLabel* diagnostic_;
};

} // namespace blcad::gui
