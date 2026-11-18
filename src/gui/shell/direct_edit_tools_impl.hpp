#pragma once
#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/shell/shell_direct_edit_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <array>
#include <map>
namespace blcad::gui {
struct ShellDirectEditTools::Impl {
  struct Binding {
    QComboBox* parameter;
    QLineEdit* value;
    ParameterType type;
  };
  ShellWindow& window;
  ShellDirectEditTools& owner;
  QAction* action{};
  QDockWidget* dock{};
  QWidget *panel{}, *overlay{};
  QScrollArea* scroll{};
  QComboBox *mode{}, *body{}, *face{}, *measurement{};
  QLineEdit *name{}, *tolerance{}, *numeric{};
  QListWidget* selected{};
  QLabel* status{};
  QPushButton *apply{}, *preview{}, *cancel{}, *add{}, *remove{}, *align{}, *world{};
  std::array<Binding, 3> move{};
  Binding angle{}, size{};
  std::array<QLineEdit*, 3> origin{}, frame_x{}, frame_y{}, axis{};
  std::array<QGroupBox*, 4> groups{};
  std::map<QString, QWidget*> fields;
  std::optional<PartDocument> inputs;
  std::optional<geometry::ShapeCache> cache;
  std::vector<geometry::SketchDrivenFaceTarget> targets;
  std::map<std::string, FaceReference> references;
  std::map<std::string, QLineEdit*> grip_fields;
  GuiViewportManipulatorLayer layer;
  bool active{}, loading{}, valid{}, editing{}, workflow{}, history_command{};
  std::size_t revision{}, prefix{};
  FeatureId feature{""};
  std::optional<GuiSelection> old_selection;
  GuiSelectionModel old_model_selection;
  std::uint32_t old_mask{};
  std::optional<std::vector<std::string>> old_scope;
  QLineEdit* drag_field{};
  QString drag_start;
  GuiSketchScreenPoint pointer;
  bool numeric_valid{true};
  Impl(ShellWindow& w, ShellDirectEditTools& o) : window(w), owner(o) {}
  bool current() const;
  void controls();
  void changed();
  void render();
  void fail(const Error&);
  void reload_faces();
  void add_face();
  void load(const DirectEditFeature*);
  void load_binding(Binding&, std::optional<ParameterId>, double sign = 1);
  void select_binding(Binding&);
  void frame_from_face(bool world_axes);
  DirectEditFrame frame() const;
  Result<ParameterId> parameter(PartDocument&, const Binding&, const char*) const;
  Result<PartDocument> candidate() const;
  Result<std::size_t> start(std::optional<FeatureId>, Mode);
  Result<std::size_t> show_inputs(const geometry::ShapeCache* = nullptr);
  void grips();
  void overlay_refresh();
  void restore_drag();
  void sample(const GuiViewportManipulatorCandidate&);
};
} // namespace blcad::gui
