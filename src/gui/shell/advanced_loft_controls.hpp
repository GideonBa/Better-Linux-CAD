#pragma once
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include <functional>
#include <map>
namespace blcad::gui {
inline std::string loft_support_key(const FaceReference& face) {
  const auto framed = [](const std::string& id) { return std::to_string(id.size()) + ":" + id; };
  return std::visit(
      [&](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ResultFaceReference>)
          return "result/" + source.identity();
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return "imported/" + framed(source.catalog().value()) + framed(source.entry().value());
        else if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return "plane/" + framed(source.source_feature().value()) +
                 framed(std::string(to_string(source.face())));
        else
          return "cylinder/" + framed(source.source_feature().value()) +
                 framed(source.source_profile().value());
      },
      face.source());
}
struct AdvancedLoftControls {
  struct Support {
    geometry::FinishingSelectionTarget target;
    BodyId owner;
    bool available = true;
  };
  struct AnchorDraft {
    QString entity, boundary, guide;
  };
  QWidget* panel{};
  QCheckBox *enabled{}, *include_hidden{};
  QComboBox* tolerance_binding{};
  QLineEdit *linear{}, *angular{}, *curvature{};
  QTableWidget *anchors{}, *supports{};
  QPushButton *add_support{}, *remove_support{}, *repair{};
  QLabel* feedback{};
  std::vector<Support> faces;
  std::vector<ProfileSectionReference> sections;
  std::vector<PathCurveId> guides;
  std::vector<bool> reversals;
  std::map<std::string, AnchorDraft> anchor_drafts;
  std::function<void()> changed;
  std::function<void(std::size_t)> face_selected;
  bool loading = false;
  explicit AdvancedLoftControls(QWidget*);
  void reset(const PartDocument&);
  void sync(const PartDocument&, std::vector<ProfileSectionReference>, std::vector<PathCurveId>,
            std::vector<bool>);
  void load(const PartDocument&, const std::optional<LoftConstraintIntent>&);
  void add_boundary(const LoftBoundaryCondition* = nullptr,
                    const LoftPatchBoundaryCondition* = nullptr, int = 0);
  [[nodiscard]] Result<std::optional<LoftConstraintIntent>> collect(ParameterId) const;
  void remember_anchors();
  void configure();
};
} // namespace blcad::gui
