#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <optional>
#include <string>

namespace blcad::gui {

enum class GuiPartFoundationCommand { Parameter, Body, Profile, Extrude, Revolve, Hole, Suppress, InspectBody };

struct GuiPartSelectionPrompt {
  std::string text;
  std::optional<GuiSelectionKind> required_kind;
  std::string required_capability;
};

struct GuiPartFeaturePreview {
  FeatureId feature;
  std::string semantic_summary;
  std::size_t affected_nodes{0};
};

struct GuiBodyInspection {
  BodyId body;
  BodyKind kind{BodyKind::Solid};
  BodyVisibility visibility{BodyVisibility::Visible};
  bool has_cached_shape{false};
  std::optional<FeatureId> source_feature;
};

class GuiPartFoundationWorkbench {
public:
  [[nodiscard]] static GuiPartSelectionPrompt prompt_for(GuiPartFoundationCommand command);

  [[nodiscard]] Result<std::size_t> create_parameter(GuiDocumentSession&, Parameter) const;
  [[nodiscard]] Result<std::size_t> create_expression_parameter(GuiDocumentSession&, ParameterId,
                                                                 std::string, ParameterType,
                                                                 std::string) const;
  [[nodiscard]] Result<std::size_t> set_parameter_value(GuiDocumentSession&, ParameterId,
                                                        Quantity) const;
  [[nodiscard]] Result<std::size_t> set_parameter_formula(GuiDocumentSession&, ParameterId,
                                                          std::string) const;
  [[nodiscard]] Result<std::size_t> create_body(GuiDocumentSession&, Body) const;
  [[nodiscard]] Result<std::size_t> activate_body(const GuiDocumentSession&, BodyId);
  [[nodiscard]] const std::optional<BodyId>& active_body() const noexcept;

  [[nodiscard]] Result<GuiPartFeaturePreview> preview_extrude(const GuiDocumentSession&,
                                                              Feature) const;
  [[nodiscard]] Result<std::size_t> apply_extrude(GuiDocumentSession&, Feature) const;
  [[nodiscard]] Result<GuiPartFeaturePreview> preview_revolve(const GuiDocumentSession&,
                                                              RevolveFeature) const;
  [[nodiscard]] Result<std::size_t> apply_revolve(GuiDocumentSession&, RevolveFeature) const;
  [[nodiscard]] Result<std::size_t> apply_hole_workflow(GuiDocumentSession&, SketchId,
                                                        CircularHolePattern, Feature) const;
  [[nodiscard]] Result<GuiBodyInspection> inspect_body(const GuiDocumentSession&, BodyId) const;
  [[nodiscard]] Result<std::size_t> suppress_feature(GuiDocumentSession&, FeatureId) const;

private:
  std::optional<BodyId> active_body_;
};

} // namespace blcad::gui
