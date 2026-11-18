#pragma once

#include "blcad/core/model_state.hpp"
#include <QObject>
#include <filesystem>
#include <memory>

class QAction;
namespace blcad::gui {
class ShellWindow;
class ShellRibbon;
class ShellModelStateTools final : public QObject {
public:
  ShellModelStateTools(ShellWindow&, ShellRibbon&, int manage_tab);
  ~ShellModelStateTools() override;
  [[nodiscard]] Result<std::size_t> begin(std::optional<DocumentId> owner = {});
  [[nodiscard]] Result<std::size_t> create_state(ModelStateId, std::string name,
                                                 bool duplicate = false);
  [[nodiscard]] Result<std::size_t> rename_state(std::string name);
  [[nodiscard]] Result<std::size_t> delete_state();
  [[nodiscard]] Result<std::size_t> select_state(ModelStateId);
  [[nodiscard]] Result<std::size_t> stage();
  [[nodiscard]] Result<std::size_t> preview();
  [[nodiscard]] Result<std::size_t> apply();
  [[nodiscard]] Result<std::size_t> activate_state(ModelStateId);
  [[nodiscard]] Result<std::uintmax_t> export_member(const std::filesystem::path&,
                                                     DocumentId new_id, std::string new_name);
  [[nodiscard]] QAction& action() noexcept;
  [[nodiscard]] bool active() const noexcept;
  void cancel();
  void refresh();

protected:
  bool eventFilter(QObject*, QEvent*) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace blcad::gui
