#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/core/hole_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"
#include "helical_curve.hpp"
#include "occt_failure_message.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Geom_ConicalSurface.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace blcad::geometry {
namespace {
constexpr double tol = 1e-7;
struct Frame {
  Point3 origin;
  Vector3 axis;
  Vector3 x;
};
gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
gp_Dir direction(Vector3 p) {
  return {p.x, p.y, p.z};
}
Point3 along(const Frame& f, double z, double r = 0) {
  return {f.origin.x + f.axis.x * z + f.x.x * r, f.origin.y + f.axis.y * z + f.x.y * r,
          f.origin.z + f.axis.z * z + f.x.z * r};
}
double axial(const Frame& f, gp_Pnt p) {
  return dot({p.X() - f.origin.x, p.Y() - f.origin.y, p.Z() - f.origin.z}, f.axis);
}
struct ExternalSupport {
  TopoDS_Face face;
  Frame frame;
};
Result<ExternalSupport> external_support(const PartDocument& part, const ShapeCache& cache,
                                         const TopoDS_Shape& target, const FaceReference& reference,
                                         const std::vector<detail::FaceLineage>& lineage,
                                         const FeatureId& id, ThreadDirection direction,
                                         StepImportTopologyContext* imports) {
  auto face = resolve_semantic_face(part, cache, target, reference, id, "Thread", imports);
  if (!face)
    return Result<ExternalSupport>::failure(face.error());
  BRepAdaptor_Surface surface(face.value());
  if (surface.GetType() != GeomAbs_Cylinder && surface.GetType() != GeomAbs_Cone)
    return Result<ExternalSupport>::failure(
        Error::geometry(id.value(), "Thread support must be cylindrical or conical"));
  const gp_Ax3 axis = surface.GetType() == GeomAbs_Cylinder ? surface.Cylinder().Position()
                                                            : surface.Cone().Position();
  double u0, u1, v0, v1;
  BRepTools::UVBounds(face.value(), u0, u1, v0, v1);
  Frame frame{{axis.Location().X(), axis.Location().Y(), axis.Location().Z()},
              {axis.Direction().X(), axis.Direction().Y(), axis.Direction().Z()},
              {axis.XDirection().X(), axis.XDirection().Y(), axis.XDirection().Z()}};
  // Preserve the same producer axis/seam rules for inspection and execution.
  for (const auto& entry : lineage)
    if (entry.face.IsSame(face.value()) && entry.support_axis) {
      frame.axis = *entry.support_axis;
      frame.x = normalized(entry.x_axis);
      break;
    }
  const double first = axial(frame, surface.Value(u0, v0)),
               last = axial(frame, surface.Value(u0, v1));
  const bool reverse = direction == ThreadDirection::OppositeSupportAxis;
  frame.origin = along(frame, reverse ? std::max(first, last) : std::min(first, last));
  if (reverse)
    frame.axis = {-frame.axis.x, -frame.axis.y, -frame.axis.z};
  return Result<ExternalSupport>::success({face.value(), frame});
}
double volume(const TopoDS_Shape& s) {
  GProp_GProps g;
  BRepGProp::VolumeProperties(s, g, 1e-9);
  return g.Mass();
}
Result<TopoDS_Shape> boolean(const FeatureId& id, const TopoDS_Shape& a, const TopoDS_Shape& b,
                             bool cut, std::vector<detail::FaceLineage>* lineage = nullptr,
                             const char* phase = "operation") {
  const auto run = [&](auto& operation) -> Result<TopoDS_Shape> {
    TopTools_ListOfShape arguments, tools;
    arguments.Append(a);
    tools.Append(b);
    operation.SetArguments(arguments);
    operation.SetTools(tools);
    operation.SetNonDestructive(true);
    operation.SetRunParallel(false);
    operation.Build();
    if (!operation.IsDone() || operation.HasErrors() || operation.Shape().IsNull() ||
        !BRepCheck_Analyzer(operation.Shape()).IsValid())
      return Result<TopoDS_Shape>::failure(
          Error::geometry(id.value(), std::string("Thread ") + phase + " is invalid"));
    if (lineage)
      *lineage = detail::map_lineage(operation, operation.Shape(), *lineage);
    return Result<TopoDS_Shape>::success(operation.Shape());
  };
  if (cut) {
    BRepAlgoAPI_Cut operation;
    return run(operation);
  }
  BRepAlgoAPI_Common operation;
  return run(operation);
}
TopoDS_Shape cone(const Frame& f, double r, double slope, double length) {
  const gp_Ax2 axis(point(f.origin), direction(f.axis), direction(f.x));
  return std::abs(slope) < 1e-12
             ? BRepPrimAPI_MakeCylinder(axis, r, length).Shape()
             : BRepPrimAPI_MakeCone(axis, r, r + slope * length, length).Shape();
}
double largest_tolerance(const TopoDS_Shape& shape) {
  double result = 0;
  for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next())
    result = std::max(result, BRep_Tool::Tolerance(TopoDS::Edge(it.Current())));
  for (TopExp_Explorer it(shape, TopAbs_VERTEX); it.More(); it.Next())
    result = std::max(result, BRep_Tool::Tolerance(TopoDS::Vertex(it.Current())));
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
    result = std::max(result, BRep_Tool::Tolerance(TopoDS::Face(it.Current())));
  return result;
}
constexpr double topology_tolerance_cap = 1e-4;
struct Applied {
  TopoDS_Shape shape;
  std::vector<detail::FaceLineage> lineage;
};
Result<Applied> apply_checked(const FeatureId& id, const TopoDS_Shape& target,
                              const TopoDS_Face& face, const ResolvedThreadDefinition& t, Frame f,
                              bool internal, std::vector<detail::FaceLineage> lineage,
                              std::string display_role) {
  const auto fail = [&](std::string message) {
    return Result<Applied>::failure(Error::geometry(id.value(), "Thread: " + std::move(message)));
  };
  BRepAdaptor_Surface surface(face);
  if (surface.GetType() != GeomAbs_Cylinder && surface.GetType() != GeomAbs_Cone)
    return fail("support must be cylindrical or conical");
  if (std::abs(length(f.axis) - 1) > tol || std::abs(length(f.x) - 1) > tol ||
      std::abs(dot(f.axis, f.x)) > tol)
    return fail("support frame must be orthonormal");
  const auto native_axis =
      surface.GetType() == GeomAbs_Cylinder ? surface.Cylinder().Axis() : surface.Cone().Axis();
  const auto native_origin = native_axis.Location();
  const auto native_direction = native_axis.Direction();
  if (std::abs(std::abs(dot(f.axis,
                            {native_direction.X(), native_direction.Y(), native_direction.Z()})) -
               1) > tol ||
      length(cross({native_origin.X() - f.origin.x, native_origin.Y() - f.origin.y,
                    native_origin.Z() - f.origin.z},
                   f.axis)) > tol)
    return fail("authored support axis no longer matches the current cylinder/cone");
  double u0, u1, v0, v1;
  BRepTools::UVBounds(face, u0, u1, v0, v1);
  if (!std::isfinite(v0) || !std::isfinite(v1))
    return fail("support must have finite axial bounds");
  const double z0 = axial(f, surface.Value(u0, v0)), z1 = axial(f, surface.Value(u0, v1));
  if (t.start_mm < std::min(z0, z1) - tol || t.start_mm + t.length_mm > std::max(z0, z1) + tol)
    return fail("start and length exceed the actual support interval");
  const double slope = std::tan(t.taper_angle_deg * std::numbers::pi / 180.);
  const auto radii = [&](double v) {
    const auto p = surface.Value(u0, v);
    const double z = axial(f, p);
    const auto c = along(f, z);
    return std::pair{z, point(c).Distance(p)};
  };
  const auto [first_z, first_r] = radii(v0);
  const auto [last_z, last_r] = radii(v1);
  const double actual_slope = (last_r - first_r) / (last_z - first_z);
  const double expected_r = (internal ? t.minor_diameter_mm : t.major_diameter_mm) / 2;
  if (!std::isfinite(actual_slope) || std::abs(actual_slope - slope) > tol ||
      std::abs(first_r + (t.start_mm - first_z) * actual_slope - expected_r) > tol)
    return fail("profile diameter or taper does not match the actual support");
  f.origin = along(f, t.start_mm);
  for (const auto& previous : lineage) {
    if (!previous.cosmetic_thread || !previous.support_axis)
      continue;
    const Vector3 delta{previous.frame_origin.x - f.origin.x, previous.frame_origin.y - f.origin.y,
                        previous.frame_origin.z - f.origin.z};
    const double alignment = dot(*previous.support_axis, f.axis);
    if (std::abs(std::abs(alignment) - 1) > tol || length(cross(delta, f.axis)) > tol)
      continue;
    const double first = dot(delta, f.axis),
                 last = first + alignment * previous.cosmetic_thread->length;
    if (std::min(t.length_mm, std::max(first, last)) - std::max(0., std::min(first, last)) > tol)
      return fail("overlapping thread definitions on the same host are unsupported");
  }
  const double a = t.minor_diameter_mm / 2, b = t.major_diameter_mm / 2, D = b - a;
  auto band = boolean(id, cone(f, b, slope, t.length_mm), cone(f, a, slope, t.length_mm), true,
                      nullptr, "envelope");
  if (!band)
    return Result<Applied>::failure(band.error());
  auto material = boolean(id, band.value(), target, false, nullptr, "material check");
  if (!material)
    return Result<Applied>::failure(material.error());
  const double expected =
      std::numbers::pi * t.length_mm * (b * b - a * a + slope * t.length_mm * (b - a));
  if (std::abs(volume(material.value()) - expected) > 1e-7 * std::max(1., expected))
    return fail(
        "complete thread envelope must fit target material without seat, bottom or side breakout");
  // Check the opposite side as well: a partial bore termination or a
  // covered cylindrical patch must not masquerade as a full thread host.
  TopoDS_Shape clearance;
  if (internal)
    clearance = cone(f, a, slope, t.length_mm);
  else {
    auto outer =
        boolean(id, cone(f, b + 1e-5, slope, t.length_mm), cone(f, b, slope, t.length_mm), true);
    if (!outer)
      return Result<Applied>::failure(outer.error());
    clearance = std::move(outer.value());
  }
  auto occupied = boolean(id, clearance, target, false, nullptr, "clearance check");
  if (!occupied)
    return Result<Applied>::failure(occupied.error());
  if (std::abs(volume(occupied.value())) > 1e-7 * std::max(1., volume(clearance)))
    return fail("support is not an exposed complete cylinder/cone throughout the interval");
  if (t.representation == ThreadRepresentation::Cosmetic) {
    lineage.push_back({{id, std::move(display_role)},
                       face,
                       f.origin,
                       f.x,
                       false,
                       f.axis,
                       detail::CosmeticThread{b, a, t.length_mm, t.pitch_mm, slope,
                                              t.handedness == ThreadHandedness::Right, internal}});
    return Result<Applied>::success({target, std::move(lineage)});
  }
  if (t.taper_angle_deg != 0)
    return fail("modeled tapered profiles are unsupported; choose cosmetic representation");
  const double half_slope = std::tan(t.included_angle_deg * std::numbers::pi / 360.);
  const double turns = t.length_mm / t.pitch_mm;
  if (!std::isfinite(turns) || turns > 32 || t.pitch_mm < .01 || t.pitch_mm > 1e6 ||
      t.length_mm < .01 || D < .01 || a < .01 || b > 1e6 || t.length_mm > 1e6 ||
      2 * D * half_slope >= t.pitch_mm - .001)
    return fail("modeled cost/profile limit: at most 32 turns, finite separated V flanks and "
                "dimensions in range");
  if (largest_tolerance(target) > topology_tolerance_cap)
    return fail("modeled input topology exceeds the 1e-4 mm tolerance cap");
  TopTools_IndexedMapOfShape input_faces;
  TopExp::MapShapes(target, TopAbs_FACE, input_faces);
  if (input_faces.Extent() > 512)
    return fail("modeled target exceeds the 512-face cost cap");
  const double margin = std::min(1e-3, (t.pitch_mm / (2 * half_slope) - D) / 4);
  const double root = internal ? b : a, far = internal ? a - margin : b + margin;
  const double half_width = (D + margin) * half_slope;
  auto edge = detail::cylindrical_helix(
      id.value(), gp_Ax3(point(f.origin), direction(f.axis), direction(f.x)), root, t.pitch_mm, 0,
      t.pitch_mm / 4, t.handedness == ThreadHandedness::Right);
  if (!edge)
    return Result<Applied>::failure(edge.error());
  BRepBuilderAPI_MakePolygon profile;
  profile.Add(point(along(f, 0, root)));
  profile.Add(point(along(f, -half_width, far)));
  profile.Add(point(along(f, half_width, far)));
  profile.Close();
  BRepOffsetAPI_MakePipeShell pipe(BRepBuilderAPI_MakeWire(edge.value()).Wire());
  // Use the exact cylindrical pcurve's surface normal. A numerical Frenet
  // frame can tilt the end section enough to leave gaps between screw copies.
  Handle(Geom2d_Curve) pcurve;
  Handle(Geom_Surface) support_surface;
  TopLoc_Location support_location;
  double first_parameter, last_parameter;
  BRep_Tool::CurveOnSurface(edge.value(), pcurve, support_surface, support_location,
                            first_parameter, last_parameter);
  if (support_surface.IsNull())
    return fail("helical pcurve support is unavailable");
  auto spine_support = BRepBuilderAPI_MakeFace(support_surface, tol).Face();
  spine_support.Location(support_location);
  if (!pipe.SetMode(spine_support))
    return fail("cannot orient the helical tool on its support");
  pipe.SetTolerance(tol, tol, tol);
  pipe.SetMaxDegree(10);
  pipe.SetMaxSegments(512);
  pipe.Add(profile.Wire(), TopExp::FirstVertex(edge.value()), false, false);
  pipe.Build();
  if (!pipe.IsDone() || !pipe.MakeSolid() || !BRepCheck_Analyzer(pipe.Shape()).IsValid())
    return fail("modeled helical tool is invalid");
  auto pipe_solid = TopoDS::Solid(pipe.Shape());
  if (!BRepLib::OrientClosedSolid(pipe_solid) || !(volume(pipe_solid) > 0))
    return fail("modeled helical tool is not a bounded positive solid");
  if (largest_tolerance(pipe_solid) > topology_tolerance_cap)
    return fail("modeled tool exceeds the 1e-4 mm topology tolerance cap");
  // A periodic cylinder cannot reliably classify one face that winds many
  // times around it. Reuse bounded quarter-turn solids with exact screw
  // placements instead; the turn cap bounds this to at most 136 pieces.
  TopoDS_Compound pieces;
  BRep_Builder builder;
  builder.MakeCompound(pieces);
  const int last_piece = static_cast<int>(std::ceil(4 * turns)) + 3;
  for (int index = -4; index <= last_piece; ++index) {
    gp_Trsf rotation, translation;
    rotation.SetRotation(gp_Ax1(point(f.origin), direction(f.axis)),
                         (t.handedness == ThreadHandedness::Right ? 1. : -1.) * index *
                             std::numbers::pi / 2);
    translation.SetTranslation(gp_Vec(f.axis.x * index * t.pitch_mm / 4,
                                      f.axis.y * index * t.pitch_mm / 4,
                                      f.axis.z * index * t.pitch_mm / 4));
    BRepBuilderAPI_Transform placed(pipe_solid, translation * rotation, false);
    if (!placed.IsDone())
      return fail("cannot place a helical tool segment");
    builder.Add(pieces, placed.Shape());
  }
  TopTools_ListOfShape first_piece, remaining_pieces;
  for (TopExp_Explorer it(pieces, TopAbs_SOLID); it.More(); it.Next()) {
    if (first_piece.IsEmpty())
      first_piece.Append(it.Current());
    else
      remaining_pieces.Append(it.Current());
  }
  BRepAlgoAPI_Fuse united;
  united.SetArguments(first_piece);
  united.SetTools(remaining_pieces);
  united.SetNonDestructive(true);
  united.SetRunParallel(false);
  united.Build();
  if (!united.IsDone() || united.HasErrors() || united.Shape().IsNull() ||
      !BRepCheck_Analyzer(united.Shape()).IsValid() ||
      largest_tolerance(united.Shape()) > topology_tolerance_cap)
    return fail("cannot join bounded helical tool segments");
  const auto joined_tool = united.Shape();
  int solid_count = 0;
  for (TopExp_Explorer it(joined_tool, TopAbs_SOLID); it.More(); it.Next())
    ++solid_count;
  if (solid_count != 1 || !(volume(joined_tool) > 0) ||
      std::abs(volume(joined_tool) - volume(pieces)) > 1e-7 * std::max(1., volume(pieces)))
    return fail("joined helical tool must be connected and preserve segment material");

  // Through-end threads use the extended tool when the entire Body is already
  // inside the authored axial slab. This avoids coincident cutter/stock end
  // caps without moving an authored limit or cutting material beyond it.
  const auto y = cross(f.axis, f.x);
  gp_Trsf local;
  local.SetValues(f.x.x, f.x.y, f.x.z, -dot({f.origin.x, f.origin.y, f.origin.z}, f.x), y.x, y.y,
                  y.z, -dot({f.origin.x, f.origin.y, f.origin.z}, y), f.axis.x, f.axis.y, f.axis.z,
                  -dot({f.origin.x, f.origin.y, f.origin.z}, f.axis));
  BRepBuilderAPI_Transform aligned(target, local, false);
  Bnd_Box bounds;
  BRepBndLib::AddOptimal(aligned.Shape(), bounds, false, false);
  double low_x, low_y, low_z, high_x, high_y, high_z;
  bounds.Get(low_x, low_y, low_z, high_x, high_y, high_z);
  const bool whole_body_interval = low_z >= -4 * tol && high_z <= t.length_mm + 4 * tol;
  auto tool = whole_body_interval ? Result<TopoDS_Shape>::success(joined_tool)
                                  : boolean(id, joined_tool, cone(f, b + 1, 0, t.length_mm), false,
                                            nullptr, "interval clip");
  if (!tool)
    return Result<Applied>::failure(tool.error());
  // Unite only the selected analytic cylinder's tangent producer patches in a
  // safe candidate. Keeping every other edge prevents unrelated simplification.
  // Four quarter-arc extrusion walls otherwise misclassify a helical tool at
  // their periodic seams. Cosmetic execution returns before this operation.
  ShapeUpgrade_UnifySameDomain unified(target, false, true, false);
  unified.SetSafeInputMode(true);
  unified.SetLinearTolerance(tol);
  TopTools_IndexedDataMapOfShapeListOfShape adjacency;
  TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, adjacency);
  for (int i = 1; i <= adjacency.Extent(); ++i) {
    bool same = true;
    for (TopTools_ListIteratorOfListOfShape it(adjacency.FindFromIndex(i)); it.More(); it.Next()) {
      BRepAdaptor_Surface candidate(TopoDS::Face(it.Value()));
      if (candidate.GetType() != GeomAbs_Cylinder) {
        same = false;
        break;
      }
      const auto cylinder = candidate.Cylinder();
      const auto p = cylinder.Location();
      const auto n = cylinder.Axis().Direction();
      const Vector3 delta{p.X() - f.origin.x, p.Y() - f.origin.y, p.Z() - f.origin.z};
      if (std::abs(cylinder.Radius() - expected_r) > tol || length(cross(delta, f.axis)) > tol ||
          std::abs(std::abs(dot({n.X(), n.Y(), n.Z()}, f.axis)) - 1) > tol) {
        same = false;
        break;
      }
    }
    if (!same)
      unified.KeepShape(adjacency.FindKey(i));
  }
  unified.Build();
  if (unified.Shape().IsNull() || !BRepCheck_Analyzer(unified.Shape()).IsValid() ||
      std::abs(volume(unified.Shape()) - volume(target)) > 1e-7 * std::max(1., volume(target)))
    return fail("cannot simplify the selected cylinder without changing material");
  lineage = detail::map_lineage(*unified.History(), unified.Shape(), lineage);
  if (largest_tolerance(tool.value()) > topology_tolerance_cap)
    return fail("clipped modeled tool exceeds the 1e-4 mm topology tolerance cap");
  auto cut = boolean(id, unified.Shape(), tool.value(), true, &lineage, "cut result");
  if (!cut)
    return Result<Applied>::failure(cut.error());
  if (!TopExp_Explorer(cut.value(), TopAbs_SOLID).More() || !(volume(cut.value()) > 0))
    return fail("modeled thread must retain a positive solid Body");
  if (!(volume(target) - volume(cut.value()) > tol))
    return fail("modeled thread removes no material");
  if (largest_tolerance(cut.value()) > topology_tolerance_cap)
    return fail("modeled result exceeds the 1e-4 mm topology tolerance cap");
  return Result<Applied>::success({std::move(cut.value()), std::move(lineage)});
}
} // namespace

Result<ThreadSupportInspection>
ThreadAdapter::inspect_support(const PartDocument& part, const BodyId& body,
                               const FaceReference& reference, const ShapeCache& cache,
                               ThreadDirection direction, StepImportTopologyContext* imports) {
  const auto fail = [&](std::string message) {
    return Result<ThreadSupportInspection>::failure(
        Error::geometry(body.value(), std::move(message)));
  };
  if (part.is_reference_part() || reference.role() != PartFeatureInputRole::ThreadSupportFace ||
      (direction != ThreadDirection::AlongSupportAxis &&
       direction != ThreadDirection::OppositeSupportAxis))
    return fail("Thread authoring requires an editable Part and a typed support/direction");
  const auto valid = validate_part_feature_input_reference(part, reference);
  if (!valid)
    return Result<ThreadSupportInspection>::failure(valid.error());
  const auto complete = cache.validate_complete_geometry_result(part);
  if (!complete)
    return Result<ThreadSupportInspection>::failure(complete.error());
  const auto* definition = part.find_body(body);
  const auto* target = cache.find_body_shape(body);
  if (!definition || definition->kind() != BodyKind::Solid || !target || target->empty())
    return fail("Thread support requires a current solid Body");
  try {
    // Inspection has no authored consumer. An empty consumer selects the current
    // terminal Body placement, including a transform that is itself terminal.
    auto resolved =
        external_support(part, cache, target->impl_->shape, reference, target->impl_->face_lineage,
                         FeatureId(""), direction, imports);
    if (!resolved)
      return fail(resolved.error().message());
    const auto& frame = resolved.value().frame;
    if (std::abs(length(frame.axis) - 1) > tol || std::abs(length(frame.x) - 1) > tol ||
        std::abs(dot(frame.axis, frame.x)) > tol)
      return fail("Thread support frame must be orthonormal");
    BRepAdaptor_Surface surface(resolved.value().face);
    double u0, u1, v0, v1;
    BRepTools::UVBounds(resolved.value().face, u0, u1, v0, v1);
    if (!std::isfinite(u0) || !std::isfinite(u1) || !std::isfinite(v0) || !std::isfinite(v1))
      return fail("Thread support must have finite bounds");
    const auto first = surface.Value(u0, v0), last = surface.Value(u0, v1);
    const double z0 = axial(frame, first), z1 = axial(frame, last), span = std::abs(z1 - z0);
    if (!(span > tol) || !std::isfinite(span))
      return fail("Thread support has no finite axial interval");
    const double r0 = point(along(frame, z0)).Distance(first),
                 r1 = point(along(frame, z1)).Distance(last);
    const double slope = (r1 - r0) / (z1 - z0), radius = r0 - slope * z0;
    if (!std::isfinite(slope) || !std::isfinite(radius) || radius < -tol ||
        radius + slope * span < -tol)
      return fail("Thread support radius or taper is invalid");
    const double u = (u0 + u1) / 2, v = (v0 + v1) / 2;
    BRepLProp_SLProps properties(surface, u, v, 1, tol);
    if (!properties.IsNormalDefined())
      return fail("Thread support normal is undefined");
    auto normal = properties.Normal();
    if (resolved.value().face.Orientation() == TopAbs_REVERSED)
      normal.Reverse();
    const auto sample = surface.Value(u, v);
    const auto center = along(frame, axial(frame, sample));
    Vector3 radial{sample.X() - center.x, sample.Y() - center.y, sample.Z() - center.z};
    if (!(length(radial) > tol) ||
        dot(normalized(radial), {normal.X(), normal.Y(), normal.Z()}) <= tol)
      return fail("Choose an exterior cylinder or cone for an external Thread");
    if (imports) {
      auto fresh = imports->validate_sources();
      if (!fresh)
        return Result<ThreadSupportInspection>::failure(fresh.error());
    }
    return Result<ThreadSupportInspection>::success({frame.origin, frame.axis, frame.x,
                                                     std::max(0., radius), span,
                                                     std::atan(slope) * 180 / std::numbers::pi});
  } catch (const Standard_Failure& e) {
    return fail(standard_failure_message(e, "Thread support inspection failed"));
  }
}

Result<GeometryShape> ThreadAdapter::apply(const PartDocument& part, const ThreadFeature& feature,
                                           const GeometryShape& target, const ShapeCache& cache,
                                           StepImportTopologyContext* imports) const {
  auto valid = validate_thread_feature_inputs(part, feature);
  if (!valid)
    return Result<GeometryShape>::failure(valid.error());
  if (target.empty())
    return Result<GeometryShape>::failure(
        Error::geometry(feature.id().value(), "Thread target is unavailable"));
  try {
    auto support =
        external_support(part, cache, target.impl_->shape, feature.support(),
                         target.impl_->face_lineage, feature.id(), feature.direction(), imports);
    if (!support)
      return Result<GeometryShape>::failure(support.error());
    auto t = resolve_thread_definition(part, feature.thread());
    if (!t)
      return Result<GeometryShape>::failure(t.error());
    auto result =
        apply_checked(feature.id(), target.impl_->shape, support.value().face, t.value(),
                      support.value().frame, false, target.impl_->face_lineage, "thread/display");
    if (!result)
      return Result<GeometryShape>::failure(result.error());
    auto backing = std::make_shared<GeometryShape::Impl>(result.value().shape);
    backing->face_lineage = std::move(result.value().lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(Error::geometry(
        feature.id().value(), standard_failure_message(e, "Thread execution failed")));
  }
}

Result<GeometryShape> ThreadAdapter::apply_hole(const PartDocument& part,
                                                const HoleFeature& feature,
                                                const GeometryShape& target, std::size_t placement,
                                                Point3 origin, Vector3 axis, Vector3 x) const {
  const auto id = feature.id();
  if (!feature.thread() || target.empty() || placement >= feature.placements().size())
    return Result<GeometryShape>::failure(
        Error::geometry(id.value(), "Threaded Hole inputs are unavailable"));
  try {
    const auto& name = feature.placements()[placement].id.value();
    const ResultFaceOrigin role{id, "hole/" + std::to_string(name.size()) + ":" + name + "/wall"};
    std::vector<TopoDS_Face> faces;
    for (const auto& e : target.impl_->face_lineage)
      if (e.origin == role &&
          std::none_of(faces.begin(), faces.end(), [&](const auto& f) { return f.IsSame(e.face); }))
        faces.push_back(e.face);
    if (faces.size() != 1)
      return Result<GeometryShape>::failure(
          Error::geometry(id.value(), "Threaded Hole wall is missing or ambiguous"));
    auto t = resolve_thread_definition(part, *feature.thread());
    if (!t)
      return Result<GeometryShape>::failure(t.error());
    auto result =
        apply_checked(id, target.impl_->shape, faces.front(), t.value(), {origin, axis, x}, true,
                      target.impl_->face_lineage, role.role + "/thread/display");
    if (!result)
      return Result<GeometryShape>::failure(result.error());
    auto backing = std::make_shared<GeometryShape::Impl>(result.value().shape);
    backing->face_lineage = std::move(result.value().lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(
        Error::geometry(id.value(), standard_failure_message(e, "Thread execution failed")));
  }
}
Result<std::vector<CosmeticThreadPresentation>>
ThreadAdapter::cosmetic_presentations(const GeometryShape& body) {
  std::vector<CosmeticThreadPresentation> output;
  if (body.empty())
    return Result<std::vector<CosmeticThreadPresentation>>::success({});
  std::vector<ResultFaceOrigin> represented;
  try {
    for (const auto& entry : body.impl_->face_lineage) {
      if (!entry.cosmetic_thread || !entry.support_axis ||
          std::find(represented.begin(), represented.end(), entry.origin) != represented.end())
        continue;
      represented.push_back(entry.origin);
      const auto& t = *entry.cosmetic_thread;
      const Frame f{entry.frame_origin, *entry.support_axis, normalized(entry.x_axis)};
      const double radius = t.internal ? t.minor_radius : t.major_radius;
      const gp_Ax3 axes(point(f.origin), direction(f.axis), direction(f.x));
      BRep_Builder builder;
      TopoDS_Compound symbol;
      builder.MakeCompound(symbol);
      for (double z : {0., t.length}) {
        const gp_Ax2 plane(point(along(f, z)), direction(f.axis), direction(f.x));
        builder.Add(symbol, BRepBuilderAPI_MakeEdge(gp_Circ(plane, radius + t.slope * z)).Edge());
      }
      // A bounded illustrative helix and interval rims, never a solid or a
      // substitute for the authored pitch/length. Sub-resolution pitch uses rims.
      if (t.pitch >= .001) {
        const double span = std::min(t.length, 4 * t.pitch);
        if (span >= .001) {
          Handle(Geom_Surface) surface;
          const double alpha = std::atan(t.slope);
          if (std::abs(t.slope) < 1e-12)
            surface = new Geom_CylindricalSurface(axes, radius);
          else
            surface = new Geom_ConicalSurface(axes, alpha, radius);
          const double q = t.pitch / (2 * std::numbers::pi * std::cos(alpha)),
                       norm = std::hypot(1., q);
          Handle(Geom2d_Line) uv =
              new Geom2d_Line(gp_Pnt2d(0, 0), gp_Dir2d((t.right ? 1. : -1.) / norm, q / norm));
          BRepBuilderAPI_MakeEdge edge(uv, surface, 0,
                                       norm * 2 * std::numbers::pi * span / t.pitch);
          if (!edge.IsDone() || !BRepLib::BuildCurves3d(edge.Edge(), tol))
            return Result<std::vector<CosmeticThreadPresentation>>::failure(Error::geometry(
                entry.origin.feature.value(), "Cannot build cosmetic Thread display"));
          builder.Add(symbol, edge.Edge());
        }
      }
      TopoDS_Compound clipped;
      builder.MakeCompound(clipped);
      for (TopExp_Explorer faces(body.impl_->shape, TopAbs_FACE); faces.More(); faces.Next()) {
        const auto face = TopoDS::Face(faces.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Cylinder && surface.GetType() != GeomAbs_Cone)
          continue;
        const auto axis = surface.GetType() == GeomAbs_Cylinder ? surface.Cylinder().Axis()
                                                                : surface.Cone().Axis();
        const auto location = axis.Location();
        const auto normal = axis.Direction();
        const Vector3 offset{location.X() - f.origin.x, location.Y() - f.origin.y,
                             location.Z() - f.origin.z};
        if (length(cross(offset, f.axis)) > tol ||
            std::abs(std::abs(dot({normal.X(), normal.Y(), normal.Z()}, f.axis)) - 1) > tol)
          continue;
        const auto p = surface.Value((surface.FirstUParameter() + surface.LastUParameter()) / 2,
                                     (surface.FirstVParameter() + surface.LastVParameter()) / 2);
        const double z = axial(f, p);
        if (std::abs(point(along(f, z)).Distance(p) - (radius + t.slope * z)) > tol)
          continue;
        auto common = boolean(entry.origin.feature, symbol, face, false);
        if (!common)
          return Result<std::vector<CosmeticThreadPresentation>>::failure(common.error());
        for (TopExp_Explorer edges(common.value(), TopAbs_EDGE); edges.More(); edges.Next())
          builder.Add(clipped, edges.Current());
      }
      if (TopExp_Explorer(clipped, TopAbs_EDGE).More()) {
        const auto& id = entry.origin.feature.value();
        const auto& role = entry.origin.role;
        output.push_back(
            {std::to_string(id.size()) + ":" + id + "/" + std::to_string(role.size()) + ":" + role,
             GeometryShape(std::make_shared<GeometryShape::Impl>(clipped))});
      }
    }
    return Result<std::vector<CosmeticThreadPresentation>>::success(std::move(output));
  } catch (const Standard_Failure& failure) {
    return Result<std::vector<CosmeticThreadPresentation>>::failure(Error::geometry(
        "thread_display",
        failure.GetMessageString() ? failure.GetMessageString() : "OCCT Thread display failed"));
  }
}
} // namespace blcad::geometry
