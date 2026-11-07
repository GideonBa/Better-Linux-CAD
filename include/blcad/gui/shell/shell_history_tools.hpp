#pragma once

#include "blcad/core/result.hpp"

#include <QObject>
#include <QTreeWidget>

#include <functional>
#include <memory>
#include <optional>
#include <string>

class QAction;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QMimeData;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace blcad::gui {
class ShellRibbon;
class ShellWindow;

struct ShellHistoryMove {
  std::string feature;
  std::string owner;
  std::optional<std::string> target;
  bool after = false;
  std::size_t revision = 0;
  bool marker = false;
};

// Dragging proposes a history transaction. The Qt model is never moved before
// the owner validates and applies the complete Part.
class ShellHistoryBrowser final : public QTreeWidget {
public:
  static constexpr int feature_role = Qt::UserRole + 3;
  static constexpr int owner_role = Qt::UserRole + 4;
  static constexpr int path_role = Qt::UserRole + 5;
  static constexpr int marker_role = Qt::UserRole + 6;
  static constexpr int marker_position_role = Qt::UserRole + 7;
  explicit ShellHistoryBrowser(QWidget* parent = nullptr);
  [[nodiscard]] bool pressed_current_item() const noexcept {
    return pressed_current_item_;
  }
  [[nodiscard]] bool dragging() const noexcept {
    return dragging_;
  }
  void set_history_callbacks(std::function<Result<std::size_t>(const ShellHistoryMove&)> check,
                             std::function<void(ShellHistoryMove)> propose,
                             std::function<std::size_t()> revision);

protected:
  QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override;
  QStringList mimeTypes() const override;
  Qt::DropActions supportedDropActions() const override;
  void dragEnterEvent(QDragEnterEvent*) override;
  void dragMoveEvent(QDragMoveEvent*) override;
  void dragLeaveEvent(QDragLeaveEvent*) override;
  void dropEvent(QDropEvent*) override;
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void startDrag(Qt::DropActions) override;

private:
  [[nodiscard]] std::optional<ShellHistoryMove> decode(const QMimeData*, QPoint point) const;
  std::function<Result<std::size_t>(const ShellHistoryMove&)> check_;
  std::function<void(ShellHistoryMove)> propose_;
  std::function<std::size_t()> revision_;
  QString nonce_;
  std::optional<int> drop_line_;
  bool pressed_current_item_ = false;
  bool dragging_ = false;
};

class ShellHistoryTools final : public QObject {
public:
  ShellHistoryTools(ShellWindow&, ShellRibbon&, int model_tab);
  ~ShellHistoryTools() override;
  [[nodiscard]] QAction& suppress_action() noexcept;
  [[nodiscard]] QAction& restore_action() noexcept;
  [[nodiscard]] QAction& reorder_action() noexcept;
  [[nodiscard]] QAction& edit_action() noexcept;
  [[nodiscard]] QAction& marker_action() noexcept;
  [[nodiscard]] QAction& marker_before_action() noexcept;
  [[nodiscard]] QAction& marker_after_action() noexcept;
  [[nodiscard]] QAction& marker_end_action() noexcept;
  [[nodiscard]] bool owns_action(const QAction*) const noexcept;
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool prefix_active() const noexcept;
  [[nodiscard]] Result<std::size_t> begin_prefix(std::size_t count);
  void append_browser_marker();
  [[nodiscard]] Result<std::size_t> begin_suppression(std::string feature, bool suppressed);
  [[nodiscard]] Result<std::size_t> begin_reorder();
  [[nodiscard]] Result<std::size_t> apply();
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
