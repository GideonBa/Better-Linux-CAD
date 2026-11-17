#pragma once
#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/shell/shell_sketch_driven_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include "sketch_driven_controls.hpp"
#include <QAction>
#include <QDockWidget>
namespace blcad::gui {
struct ShellSketchDrivenTools::Impl {
  ShellWindow& window;
  ShellSketchDrivenTools& owner;
  std::unique_ptr<detail::SketchDrivenControls> ui;
  QDockWidget* dock{};
  std::array<QAction*, 4> actions{};
  GuiViewportManipulatorLayer layer;
  QWidget* overlay{};
  bool active{}, loading{}, valid{}, editing{}, workflow{}, history_command{}, repair{};
  std::size_t revision{}, index{}, prefix{};
  FeatureId feature{""};
  std::optional<SketchId> source_edit;
  std::optional<PartDocument> inputs;
  std::optional<geometry::ShapeCache> input_cache;
  std::vector<geometry::SketchDrivenFaceTarget> targets;
  std::map<std::string, FaceReference> face_references;
  std::map<std::string, QLineEdit*> grip_fields;
  std::optional<GuiSelection> old_selection;
  std::uint32_t old_mask{};
  std::optional<std::vector<std::string>> old_scope;
  std::optional<EmbossTextSource> original_text;
  QString drag_start;
  bool numeric_valid = true;
  GuiSketchScreenPoint grip_pointer;
  QLineEdit* drag_field{};
  Impl(ShellWindow& w, ShellSketchDrivenTools& o) : window(w), owner(o) {}
  bool current() const;
  void edited();
  void fail(const Error&);
  void guides(const PartDocument&, const SketchDrivenFeature&);
  Result<std::size_t> start(std::optional<FeatureId>, Mode);
  Result<std::size_t> prepare_inputs(const PartDocument&, std::size_t);
  void reload_sources(bool clear);
  void reload_targets();
  void add_curve();
  void add_face();
  void add_token(const SketchTextBinding* = nullptr);
  void load(const SketchDrivenFeature*);
  void bind(detail::SketchDrivenControls::Binding&, std::optional<ParameterId>);
  Result<PartDocument> candidate() const;
  Result<ParameterId> parameter(PartDocument&, const detail::SketchDrivenControls::Binding&,
                                const char*) const;
  void overlay_refresh();
  Result<std::size_t> show_inputs(const geometry::GeometryShape* = nullptr);
  void apply_grip(const GuiViewportManipulatorCandidate&);
  void restore_grip();
};
} // namespace blcad::gui
