#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/gui_types.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::geometry {
class ShapeCache;
}

namespace blcad::gui {

struct GuiSketchScreenPoint {
  double x{0.0};
  double y{0.0};
  friend bool operator==(const GuiSketchScreenPoint&, const GuiSketchScreenPoint&) = default;
};

struct GuiSketchScreenSegment {
  GuiSketchScreenPoint start;
  GuiSketchScreenPoint end;
  bool major{false};
};

struct GuiSketchScreenRect {
  GuiSketchScreenPoint start;
  GuiSketchScreenPoint end;

  [[nodiscard]] double left() const noexcept;
  [[nodiscard]] double right() const noexcept;
  [[nodiscard]] double top() const noexcept;
  [[nodiscard]] double bottom() const noexcept;
  [[nodiscard]] bool contains(GuiSketchScreenPoint point) const noexcept;
};

struct GuiSketchViewRay {
  Point3 origin;
  Vector3 direction;
};

class GuiSketchPlaneMapping {
public:
  using ScreenToRay = std::function<Result<GuiSketchViewRay>(GuiSketchScreenPoint)>;
  using ModelToScreen = std::function<Result<GuiSketchScreenPoint>(Point3)>;

  [[nodiscard]] static Result<GuiSketchPlaneMapping>
  create(GuiSketchPlaneView plane, ScreenToRay screen_to_ray, ModelToScreen model_to_screen);

  [[nodiscard]] static Result<GuiSketchPlaneMapping>
  create_orthographic(GuiSketchPlaneView plane, double viewport_width_dip,
                      double viewport_height_dip, Point2 plane_center,
                      double model_units_per_dip);

  [[nodiscard]] const GuiSketchPlaneView& plane() const noexcept;
  [[nodiscard]] Result<GuiSketchViewRay> screen_to_ray(GuiSketchScreenPoint point) const;
  [[nodiscard]] Result<Point2> screen_to_plane(GuiSketchScreenPoint point) const;
  [[nodiscard]] Result<Point3> screen_to_model(GuiSketchScreenPoint point) const;
  [[nodiscard]] Result<GuiSketchScreenPoint> plane_to_screen(Point2 point) const;
  [[nodiscard]] Result<GuiSketchScreenPoint> model_to_screen(Point3 point) const;

private:
  GuiSketchPlaneMapping(GuiSketchPlaneView plane, ScreenToRay screen_to_ray,
                        ModelToScreen model_to_screen);

  GuiSketchPlaneView plane_;
  ScreenToRay screen_to_ray_;
  ModelToScreen model_to_screen_;
};

enum class GuiSketchCurveKind { Line, Arc, Spline, Circle, ReferenceLine };
enum class GuiSketchHitKind { Point, Curve, Dimension, Glyph };
enum class GuiSketchSnapKind {
  None,
  Origin,
  Axis,
  Endpoint,
  Midpoint,
  Center,
  Quadrant,
  Intersection,
  Nearest,
  Grid,
  HorizontalInference,
  VerticalInference,
  AlignmentX,
  AlignmentY,
};
enum class GuiSketchBoxSelectionMode { Window, Crossing };

[[nodiscard]] std::string_view to_string(GuiSketchHitKind kind) noexcept;
[[nodiscard]] std::string_view to_string(GuiSketchSnapKind kind) noexcept;
[[nodiscard]] std::string_view to_string(GuiSketchBoxSelectionMode mode) noexcept;

struct GuiSketchCurvePrimitive {
  std::string semantic_id;
  std::string candidate_id;
  GuiSketchCurveKind kind{GuiSketchCurveKind::Line};
  std::vector<Point2> polyline;
  bool closed{false};
};

struct GuiSketchPointPrimitive {
  std::string semantic_id;
  std::string candidate_id;
  Point2 point;
  GuiSketchSnapKind snap_kind{GuiSketchSnapKind::Endpoint};
};

struct GuiSketchAnnotationPrimitive {
  std::string semantic_id;
  std::string candidate_id;
  Point2 point;
  GuiSketchHitKind hit_kind{GuiSketchHitKind::Dimension};
};

struct GuiSketchInteractionScene {
  SketchId sketch{SketchId("sketch.interaction")};
  std::vector<GuiSketchCurvePrimitive> curves;
  std::vector<GuiSketchPointPrimitive> points;
  std::vector<GuiSketchAnnotationPrimitive> annotations;
  std::vector<Point2> intersections;
  std::size_t unresolved_reference_count{0};
};

class GuiSketchInteractionSceneBuilder {
public:
  [[nodiscard]] Result<GuiSketchInteractionScene>
  build(const PartDocument& document, const Sketch& sketch) const;
  [[nodiscard]] Result<GuiSketchInteractionScene>
  build(const PartDocument& document, const Sketch& sketch,
        const geometry::ShapeCache& shape_cache) const;

private:
  [[nodiscard]] Result<GuiSketchInteractionScene>
  build_impl(const PartDocument& document, const Sketch& sketch,
             const geometry::ShapeCache* shape_cache) const;
};

struct GuiSketchHit {
  GuiSketchHitKind kind{GuiSketchHitKind::Curve};
  std::string semantic_id;
  std::string candidate_id;
  Point2 plane_point;
  double model_distance{0.0};
  double screen_distance_dip{0.0};
};

struct GuiSketchSnapResult {
  Point2 raw_point;
  Point2 snapped_point;
  GuiSketchSnapKind kind{GuiSketchSnapKind::None};
  std::string candidate_id;
  std::string inference;
  double model_distance{0.0};
  double screen_distance_dip{0.0};
};

struct GuiSketchGridConfig {
  bool visible{true};
  bool snap_enabled{true};
  double spacing{10.0};
  std::size_t major_every{5};
};

struct GuiSketchInteractionConfig {
  double point_hit_tolerance_dip{8.0};
  double curve_hit_tolerance_dip{6.0};
  double annotation_hit_tolerance_dip{8.0};
  double snap_tolerance_dip{10.0};
  double stacked_hit_reset_tolerance_dip{3.0};
  GuiSketchGridConfig grid;
};

class GuiSketchInteractionController {
public:
  [[nodiscard]] static Result<GuiSketchInteractionController>
  create(GuiSketchPlaneMapping mapping, GuiSketchInteractionScene scene,
         GuiSketchInteractionConfig config = {});

  [[nodiscard]] const GuiSketchPlaneMapping& mapping() const noexcept;
  [[nodiscard]] const GuiSketchInteractionScene& scene() const noexcept;
  [[nodiscard]] const GuiSketchInteractionConfig& config() const noexcept;
  void set_config(GuiSketchInteractionConfig config);

  [[nodiscard]] Result<std::vector<GuiSketchHit>>
  hits_at(GuiSketchScreenPoint screen_point) const;
  [[nodiscard]] Result<std::optional<GuiSketchHit>>
  cycle_hit(GuiSketchScreenPoint screen_point);
  void reset_hit_cycle() noexcept;

  [[nodiscard]] Result<GuiSketchSnapResult>
  snap(GuiSketchScreenPoint screen_point,
       std::optional<Point2> inference_anchor = std::nullopt) const;

  [[nodiscard]] Result<std::vector<GuiSelection>>
  box_select(GuiSketchScreenRect rectangle, GuiSketchBoxSelectionMode mode) const;

  [[nodiscard]] Result<std::vector<GuiSketchScreenSegment>>
  grid_lines(double viewport_width_dip, double viewport_height_dip,
             std::size_t maximum_lines = 512) const;

  [[nodiscard]] Result<std::vector<GuiSketchScreenPoint>>
  screen_polyline(const GuiSketchCurvePrimitive& curve) const;

private:
  GuiSketchInteractionController(GuiSketchPlaneMapping mapping,
                                 GuiSketchInteractionScene scene,
                                 GuiSketchInteractionConfig config);

  GuiSketchPlaneMapping mapping_;
  GuiSketchInteractionScene scene_;
  GuiSketchInteractionConfig config_;
  std::optional<GuiSketchScreenPoint> last_cycle_point_;
  std::vector<std::string> last_cycle_signature_;
  std::size_t cycle_index_{0};
};

} // namespace blcad::gui
