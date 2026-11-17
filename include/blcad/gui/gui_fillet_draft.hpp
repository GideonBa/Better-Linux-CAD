#pragma once
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include <map>
#include <optional>
#include <vector>
namespace blcad::gui {
// One candidate shared by the owned controls and canvas interaction. Applying it always
// runs inside the existing document transaction; Core remains the only owned authority.
struct GuiFilletDraft {
  FilletFeature record;
  bool editing = false, independent_values = false;
  std::vector<Parameter> pending;
  std::map<std::string, double> values;
  std::vector<GuiViewportManipulatorHandle> handles;
  std::map<std::string, ParameterId> handle_bindings;
  [[nodiscard]] Result<std::size_t> apply(PartDocument&) const;
  [[nodiscard]] double value(const PartDocument&, const ParameterId&) const;
  [[nodiscard]] bool move(const GuiViewportManipulatorCandidate&);
};
} // namespace blcad::gui
