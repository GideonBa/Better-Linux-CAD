#pragma once
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/geometry/assembly_occurrence_solver.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/shell/shell_advanced_assembly_tools.hpp"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>

namespace blcad::gui {
struct ShellAdvancedAssemblyTools::Impl {
  ShellWindow& window;
  ShellAdvancedAssemblyTools& owner;
  Impl(ShellWindow& shell, ShellAdvancedAssemblyTools& tools) : window(shell), owner(tools) {}
  Mode mode{Mode::Flexible};
  std::vector<std::pair<Mode, QAction*>> actions;
  QDockWidget* dock{};
  QWidget* breadcrumb{};
  QLabel *trail{}, *summary{}, *dependencies{};
  QStackedWidget* pages{};
  QComboBox *record{}, *occurrence{}, *flexibility{}, *joint{}, *coordinate{}, *source{},
      *source_state{}, *geometry_editor{}, *target{}, *construction{}, *layout{}, *layout_state{},
      *pattern{}, *positions{}, *views{}, *context_part{};
  QDoubleSpinBox* value{};
  QLineEdit* name{};
  QTableWidget *participants{}, *display{};
  QPushButton *preview{}, *apply{}, *detach{};
  std::vector<AssemblyHierarchyOccurrenceDescriptor> subassemblies;
  std::vector<AssemblyLeafOccurrenceDescriptor> leaves;
  struct SourceChoice {
    ProjectDesignSource source;
    int kind;
    QString label;
  };
  struct LayoutChoice {
    ProjectDesignOccurrence occurrence;
    FeatureId hole;
  };
  std::vector<SourceChoice> source_choices;
  std::vector<LayoutChoice> layouts;
  std::optional<Project> candidate;
  std::vector<AssemblyPosition> draft_positions;
  std::vector<AssemblyDesignView> draft_views;
  std::vector<AssemblyComponentDisplay> display_components;
  std::vector<AssemblySubassemblyDisplay> display_subassemblies;
  AssemblyDesignViewId preview_view;
  ProjectDesignReferenceId link_id;
  FeatureId hole_id;
  std::size_t revision{};
  std::uint32_t prior_mask{};
  std::optional<std::vector<std::string>> prior_scope;
  bool active = false, loading = false;
  [[nodiscard]] bool available(Mode) const;
  [[nodiscard]] bool current() const;
  void invalidate();
  void load();
  void load_flexible();
  void load_coordinate();
  void load_source();
  void load_target();
  void load_layout();
  void load_record();
  void load_view();
  void update_dependencies();
  [[nodiscard]] const AssemblyLeafOccurrenceDescriptor* selected(QComboBox*) const;
  [[nodiscard]] Result<std::size_t> mutate(Project&);
  [[nodiscard]] Result<std::size_t> publish_preview(GuiDocumentSession&);
  [[nodiscard]] Result<std::size_t> edit_part();
  [[nodiscard]] Result<std::size_t> detach_record();
  [[nodiscard]] QString occurrence_label(const AssemblyLeafOccurrenceDescriptor&) const;
  [[nodiscard]] std::string label() const;
};
} // namespace blcad::gui
