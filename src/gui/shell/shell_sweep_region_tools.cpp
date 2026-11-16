#include "blcad/geometry/sweep_region_query.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QListWidget>
namespace blcad::gui {
void ShellFeatureTools::build_sweep_region_controls(QFormLayout& form) {
  sweep_region_policy_ = new QComboBox(spatial_controls_);
  sweep_region_policy_->setObjectName("blcad.sweep.region_policy");
  sweep_region_policy_->addItems({"Outer Boundary Only", "Material Region — Preserve Holes"});
  sweep_region_policy_->setToolTip(
      "Material Region uses every immediate hole of the selected region. Touching or intersecting "
      "boundaries must be corrected.");
  form.addRow("Profile Region", sweep_region_policy_);
  sweep_preserved_holes_ = new QListWidget(spatial_controls_);
  sweep_preserved_holes_->setObjectName("blcad.sweep.preserved_holes");
  sweep_preserved_holes_->setMinimumHeight(65);
  sweep_preserved_holes_->setMaximumHeight(100);
  sweep_preserved_holes_->setSelectionMode(QAbstractItemView::NoSelection);
  form.addRow("Preserved Holes", sweep_preserved_holes_);
  connect(sweep_region_policy_, &QComboBox::currentIndexChanged, this, [this] {
    if (command_ == Command::Sweep)
      refresh_spatial_preview();
  });
}
Result<SweepProfileReference> ShellFeatureTools::selected_sweep_profile() {
  sweep_preserved_holes_->clear();
  const int index = spatial_source_->currentData().toInt();
  if (index < 0 || std::size_t(index) >= spatial_inputs_.size())
    return Result<SweepProfileReference>::failure(
        Error::validation("sweep", "Choose a profile region or open path"));
  const auto& source = spatial_inputs_[index];
  if (!source.profile) {
    if (source.path)
      return SweepProfileReference::create_open_path(*source.path);
    return Result<SweepProfileReference>::failure(
        Error::validation("sweep", "Choose a profile region or open path"));
  }
  auto reference = ProfileRegionReference::create(source.profile->first, source.profile->second,
                                                  PartFeatureInputRole::SweepProfile);
  if (!reference)
    return Result<SweepProfileReference>::failure(reference.error());
  const auto policy = sweep_region_policy_->currentIndex() == 1 ? SweepRegionPolicy::MaterialRegion
                                                                : SweepRegionPolicy::OuterBoundary;
  if (policy == SweepRegionPolicy::MaterialRegion) {
    auto holes = geometry::inspect_sweep_material_holes(
        *session_.part_document(), source.profile->first, source.profile->second);
    if (!holes) {
      sweep_preserved_holes_->addItem(QString::fromStdString(holes.error().message()));
      return Result<SweepProfileReference>::failure(holes.error());
    }
    for (const auto& hole : holes.value())
      sweep_preserved_holes_->addItem(QString::fromStdString(hole.value()));
    if (holes.value().empty())
      sweep_preserved_holes_->addItem("No contained holes");
  }
  return SweepProfileReference::create_closed_region(reference.value(), policy);
}
Result<std::vector<ShellFeatureTools::SpatialNativeEdge>>
ShellFeatureTools::native_spatial_edges() const {
  return query_with_step_import_context(
      session_, [&](auto* context) -> Result<std::vector<SpatialNativeEdge>> {
        std::vector<SpatialNativeEdge> result;
        const auto* part = session_.part_document();
        const auto* cache = session_.part_shape_cache();
        if (!part || !cache)
          return Result<std::vector<SpatialNativeEdge>>::success({});
        auto observed = geometry::observe_imported_part_sources(*part, context);
        if (!observed)
          return Result<std::vector<SpatialNativeEdge>>::failure(observed.error());
        for (const auto& body : part->bodies()) {
          if (body.visibility() != BodyVisibility::Visible ||
              !session_.presentation_visible("body/" + body.id().value()) ||
              !cache->find_body_shape(body.id()))
            continue;
          auto edges = geometry::FinishingSelectionBuilder::build(
              *part, *cache, body.id(), PartFeatureInputRole::PathSegment, context,
              session_.history_prefix_count());
          if (!edges)
            return Result<std::vector<SpatialNativeEdge>>::failure(edges.error());
          for (auto& edge : edges.value())
            result.push_back({std::move(edge), body.id()});
        }
        return Result<std::vector<SpatialNativeEdge>>::success(std::move(result));
      });
}
} // namespace blcad::gui
