#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/loft_section_refinement_fixture.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/gui/gui_interactive_path_sweep_loft.hpp"
#include "blcad/gui/gui_interactive_surface.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace {
std::string saved(const GuiDocumentSession& session) {
  return blcad::test::surface_offset::saved(*session.part_document());
}
void inputs(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("surface.modern"), "Modern Surface"));
  REQUIRE(session.commit_part_transaction("Inputs", [](PartDocument& p) {
    p = f::part();
    return Result<std::size_t>::success(1);
  }));
}
} // namespace
TEST_CASE("Advanced Surface creation and editing share signed thickness "
          "candidates and exact history",
          "[gui][advanced-surface-controller][parity-b173]") {
  GuiDocumentSession session;
  inputs(session);
  const auto original = saved(session);
  const SurfaceOffsetParameters parameters{ParameterId("wall"),
                                           ParameterId("tolerance"),
                                           SurfaceOffsetSide::Symmetric,
                                           SurfaceOffsetApproximation::ExactAnalytic,
                                           {}};
  const auto context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, {}, BodyId("modern.body")));
  GuiInteractiveSurfaceController controller;
  REQUIRE(controller.begin_offset(
      session, FeatureId("modern"), "Modern", SurfaceFeatureKind::Thicken, f::quadrant({0}),
      parameters, context,
      f::checked(Body::create(BodyId("modern.body"), "Modern", BodyKind::Solid))));
  controller.set_manipulator_frame({{10, 0, 10}, {1, 0, 0}});
  REQUIRE(controller.handles().size() == 1);
  CHECK(controller.handles().front().id == kSurfaceOffsetHandleId);
  CHECK(controller.handles().front().model_units_per_value == .5);
  REQUIRE(controller.preview(session));
  CHECK(saved(session) == original);
  REQUIRE(controller.apply_manipulator({std::string(kSurfaceOffsetHandleId),
                                        GuiViewportManipulatorValueKind::Distance,
                                        GuiViewportManipulatorInputSource::Drag, 2}));
  REQUIRE(controller.apply(session));
  const auto created = saved(session);
  const auto created_generation = session.part_document()->executable_generation();
  const auto created_shape = *session.part_shape_cache()->find_body_shape(BodyId("modern.body"));
  CHECK(blcad::test::integrated_solid_volume(
            *session.part_shape_cache()->find_body_shape(BodyId("modern.body")))
            .volume_mm3 == Catch::Approx(200 * std::numbers::pi).margin(1e-5));
  REQUIRE(controller.begin_edit(session, FeatureId("modern")));
  CHECK(controller.editing());
  controller.set_offset_side(SurfaceOffsetSide::OppositeNormal);
  CHECK(controller.handles().front().axis == Vector3{-1, 0, 0});
  CHECK(controller.handles().front().model_units_per_value == 1);
  controller.set_distance(3);
  REQUIRE(controller.apply(session));
  const auto edited = saved(session);
  const auto edited_generation = session.part_document()->executable_generation();
  const auto edited_shape = *session.part_shape_cache()->find_body_shape(BodyId("modern.body"));
  CHECK(edited_generation > created_generation);
  CHECK(blcad::test::integrated_solid_volume(
            *session.part_shape_cache()->find_body_shape(BodyId("modern.body")))
            .volume_mm3 == Catch::Approx(255 * std::numbers::pi).margin(1e-5));
  REQUIRE(session.undo());
  CHECK(saved(session) == created);
  CHECK(session.part_document()->executable_generation() == created_generation);
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  CHECK(geometry::OcctShapeView::get(*session.part_shape_cache()->find_body_shape(
            BodyId("modern.body"))) == geometry::OcctShapeView::get(created_shape));
  REQUIRE(session.undo());
  CHECK(saved(session) == original);
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  REQUIRE(session.redo());
  CHECK(session.part_document()->executable_generation() == created_generation);
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  REQUIRE(session.redo());
  CHECK(saved(session) == edited);
  CHECK(session.part_document()->executable_generation() == edited_generation);
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  CHECK(geometry::OcctShapeView::get(*session.part_shape_cache()->find_body_shape(
            BodyId("modern.body"))) == geometry::OcctShapeView::get(edited_shape));
  REQUIRE(controller.begin_edit(session, FeatureId("modern")));
  controller.set_distance(20);
  REQUIRE_FALSE(controller.apply(session));
  CHECK(saved(session) == edited);
  CHECK(controller.active());
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  controller.set_distance(2);
  const auto corrected = controller.apply(session);
  INFO((corrected ? std::string("corrected thickness accepted") : corrected.error().message()));
  REQUIRE(corrected);
  CHECK(session.part_shape_cache()->validate_complete_geometry_result(*session.part_document()));
  CHECK(blcad::test::integrated_solid_volume(
            *session.part_shape_cache()->find_body_shape(BodyId("modern.body")))
            .volume_mm3 == Catch::Approx(180 * std::numbers::pi).margin(1e-5));
}
TEST_CASE("Modern Trim and Extend options share creation/edit controls without "
          "losing parameter formulas",
          "[gui][advanced-surface-controller][parity-b173]") {
  GuiDocumentSession session;
  inputs(session);
  GuiInteractiveSurfaceController controller;
  const auto trim = f::checked(TrimmingReference::create_profile_region(
      f::checked(ProfileRegionReference::create(SketchId("window.sketch"), ProfileId("window")))));
  REQUIRE(controller.begin_trim(
      session, FeatureId("trim"), "Trim", f::quadrant(), trim, BodyId("trim.body"),
      f::checked(Body::create(BodyId("trim.body"), "Trim", BodyKind::Surface))));
  controller.set_trim_options(SurfaceTrimOptions{SurfaceTrimSide::KeepInside,
                                                 SurfaceTrimProjection::AlongDirection,
                                                 Vector3{0, 1, 0}, ParameterId("tolerance")});
  REQUIRE(controller.apply(session));
  const auto original = saved(session);
  REQUIRE(controller.begin_edit(session, FeatureId("trim")));
  controller.set_trim_options(SurfaceTrimOptions{SurfaceTrimSide::RemoveInside,
                                                 SurfaceTrimProjection::AlongDirection,
                                                 Vector3{0, 1, 0}, ParameterId("tolerance")});
  REQUIRE(controller.apply(session));
  REQUIRE(session.undo());
  CHECK(saved(session) == original);
  REQUIRE(session.commit_part_transaction("Boundary", [](PartDocument& p) {
    f::arc_path(p, "top", 0, std::acos(.8), "window.top");
    const auto expression =
        p.add_expression_parameter(ParameterId("formula.extension"), "formula_extension",
                                   ParameterType::Length, "extension * 2");
    if (!expression)
      return Result<std::size_t>::failure(expression.error());
    return Result<std::size_t>::success(1);
  }));
  const auto feature = f::checked(ExtendSurfaceFeature::create(
      FeatureId("extend"), "Extend", f::body("trim.body"),
      f::checked(BoundaryCurveReference::create_path_curve(PathCurveId("top"))),
      ParameterId("formula.extension"), BodyId("extended.body")));
  REQUIRE(controller.begin_extend(
      session, feature.id(), feature.name(), feature.target(), feature.boundary(),
      feature.distance_parameter(), feature.result_body(),
      f::checked(Body::create(feature.result_body(), "Extended", BodyKind::Surface))));
  controller.set_extend_options(
      SurfaceExtendOptions{SurfaceExtendMode::Natural, ParameterId("tolerance"), {}});
  REQUIRE(controller.apply(session));
  const auto before = saved(session);
  REQUIRE(controller.begin_edit(session, feature.id()));
  REQUIRE(controller.apply(session));
  CHECK(saved(session) == before);
  CHECK(session.part_document()
            ->find_parameter(ParameterId("formula.extension"))
            ->formula()
            .has_value());
}

TEST_CASE(
    "Advanced Loft candidates retain explicit G2 support intent and measured boundary feedback",
    "[gui][advanced-loft-controller][parity-b173]") {
  auto model = blcad::test::constrained_airfoil_fixture();
  const auto recipe = *model.find_loft_feature(FeatureId("main"));
  REQUIRE(model.remove_loft_feature(recipe.id()));
  REQUIRE(model.remove_body(BodyId("main")));
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("loft.modern"), "Modern Loft"));
  REQUIRE(session.commit_part_transaction("Inputs", [&](PartDocument& p) {
    p = model;
    return Result<std::size_t>::success(1);
  }));
  const auto original = saved(session);
  GuiInteractiveLoftController controller;
  REQUIRE(controller.begin_loft(
      session, FeatureId("modern.loft"), "Modern Loft", LoftFeatureKind::Loft,
      BodyId("modern.loft.body"),
      f::checked(Body::create(BodyId("modern.loft.body"), "Modern Loft", BodyKind::Solid))));
  controller.set_auto_alignment(false);
  for (const auto& section : recipe.sections()) {
    const auto& profile = std::get<ProfileRegionReference>(section.source());
    REQUIRE(
        controller.add_closed_section(profile.sketch(), profile.profile(), section.flip_normal()));
  }
  controller.set_path_curve(recipe.path_curve());
  for (const auto& guide : recipe.guide_curves())
    REQUIRE(controller.add_guide_curve(guide));
  controller.set_constraints(recipe.constraints());
  controller.set_continuity(LoftContinuity::G2);
  const auto unsupported = controller.preview_shape(session);
  REQUIRE_FALSE(unsupported);
  CHECK(unsupported.error().message().find("longitudinal") != std::string::npos);
  CHECK(saved(session) == original);
  controller.set_continuity(LoftContinuity::C0);
  const auto preview = f::checked(controller.preview_shape(session));
  const auto report = blcad::geometry::LoftAdapter::verification(preview);
  REQUIRE(report);
  CHECK(report->g2_joins == 4);
  CHECK(report->g1_joins == 0);
  CHECK(report->stations >= 36);
  CHECK(report->curve_error_mm <= report->linear_tolerance_mm);
  CHECK(report->position_error_mm <= report->linear_tolerance_mm);
  CHECK(report->normal_error_rad <= report->angular_tolerance_rad);
  CHECK(report->curvature_error_per_mm <= report->curvature_tolerance_per_mm);
  CHECK(report->before_body_operation);
  CHECK(saved(session) == original);
  for (std::size_t station = 0; station < 3; ++station)
    for (const bool upper : {false, true})
      for (int i = 0; i <= 8; ++i) {
        const auto point = blcad::test::loft_refinement::model(
            station, blcad::test::loft_refinement::at(
                         blcad::test::loft_refinement::poles(station, upper), i / 8.0));
        CHECK(blcad::test::distance_to_faces(preview, point) < 1e-5);
      }
  REQUIRE(controller.apply(session));
  const auto edited = saved(session);
  REQUIRE(session.undo());
  CHECK(saved(session) == original);
  REQUIRE(session.redo());
  CHECK(saved(session) == edited);
}
