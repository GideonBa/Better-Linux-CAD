#include "blcad/geometry/pattern_placement_adapter.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax3.hxx>

namespace blcad::geometry {
namespace {
double volume(const TopoDS_Shape& shape) {
  GProp_GProps properties;
  BRepGProp::VolumeProperties(shape, properties, 1e-9);
  return properties.Mass();
}
template <class Operation>
Result<TopoDS_Shape> operation(FeatureId id, const TopoDS_Shape& a, const TopoDS_Shape& b,
                               std::vector<detail::FaceLineage>& lineage) {
  Operation tool;
  TopTools_ListOfShape arguments, tools;
  arguments.Append(a);
  tools.Append(b);
  tool.SetArguments(arguments);
  tool.SetTools(tools);
  tool.SetNonDestructive(true);
  tool.SetRunParallel(false);
  tool.Build();
  if (!tool.IsDone() || tool.HasErrors() || tool.Shape().IsNull() ||
      !BRepCheck_Analyzer(tool.Shape()).IsValid())
    return Result<TopoDS_Shape>::failure(
        Error::geometry(id.value(), "Hole Pattern boolean is invalid"));
  lineage = detail::map_lineage(tool, tool.Shape(), lineage);
  return Result<TopoDS_Shape>::success(tool.Shape());
}
gp_Ax3 axes(const HoleInstanceFrame& f) {
  const auto& p = f.frame.origin;
  const auto& n = f.frame.normal;
  const auto& x = f.frame.x_axis;
  return {gp_Pnt(p.x, p.y, p.z), gp_Dir(n.x, n.y, n.z), gp_Dir(x.x, x.y, x.z)};
}
double largest_tolerance(const TopoDS_Shape& shape) {
  double value = 0;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
    value = std::max(value, BRep_Tool::Tolerance(TopoDS::Face(it.Current())));
  for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next())
    value = std::max(value, BRep_Tool::Tolerance(TopoDS::Edge(it.Current())));
  for (TopExp_Explorer it(shape, TopAbs_VERTEX); it.More(); it.Next())
    value = std::max(value, BRep_Tool::Tolerance(TopoDS::Vertex(it.Current())));
  return value;
}
} // namespace
Result<GeometryShape> PatternPlacementAdapter::place(const GeometryShape& source,
                                                     const gp_Trsf& transform) {
  if (source.empty())
    return Result<GeometryShape>::failure(
        Error::geometry("pattern", "Source shape is unavailable"));
  try {
    BRepBuilderAPI_Transform placed(source.impl_->shape, transform, false);
    if (!placed.IsDone() || placed.Shape().IsNull())
      return Result<GeometryShape>::failure(
          Error::geometry("pattern", "Cannot place source geometry"));
    auto backing = std::make_shared<GeometryShape::Impl>(placed.Shape());
    backing->face_lineage = detail::map_lineage(placed, placed.Shape(), source.impl_->face_lineage);
    detail::transform_frames(backing->face_lineage, transform);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(
        Error::geometry("pattern", standard_failure_message(e, "Cannot place source geometry")));
  }
}
Result<std::size_t>
PatternPlacementAdapter::validate_holes(const PartDocument& part,
                                        const std::vector<PatternSourceReference>& sources,
                                        std::size_t occurrences) {
  std::size_t bores = 0, modeled = 0;
  double turns = 0;
  for (const auto& source : sources) {
    const auto* hole = source.kind() == PatternSourceKind::Feature
                           ? part.find_hole_feature(std::get<FeatureId>(source.source()))
                           : nullptr;
    if (!hole)
      return Result<std::size_t>::failure(
          Error::geometry("pattern", "Hole Patterns require only Hole sources"));
    if (std::holds_alternative<HoleSlot>(hole->manufacturing()) ||
        (!std::holds_alternative<HoleBlindExtent>(hole->extent()) &&
         !std::holds_alternative<HoleThroughAllExtent>(hole->extent())))
      return Result<std::size_t>::failure(Error::geometry(
          hole->id().value(), "Identical Hole Patterns support round Blind and Through All seeds; "
                              "other terminations require a recomputed recipe"));
    if (occurrences > 256 / hole->placements().size() ||
        bores > 256 - occurrences * hole->placements().size())
      return Result<std::size_t>::failure(
          Error::geometry(hole->id().value(), "Hole Pattern exceeds 256 logical bores"));
    bores += occurrences * hole->placements().size();
    if (hole->thread()) {
      auto t = resolve_thread_definition(part, *hole->thread());
      if (!t)
        return Result<std::size_t>::failure(t.error());
      if (t.value().representation == ThreadRepresentation::Modeled) {
        modeled += occurrences * hole->placements().size();
        turns += occurrences * hole->placements().size() * t.value().length_mm / t.value().pitch_mm;
      }
    }
  }
  if (modeled > 16 || !std::isfinite(turns) || turns > 128)
    return Result<std::size_t>::failure(
        Error::geometry("pattern", "Modeled Hole Pattern exceeds 16 bores or 128 aggregate turns"));
  return Result<std::size_t>::success(bores);
}
Result<GeometryShape>
PatternPlacementAdapter::repeat_hole(const PartDocument& part, FeatureId pattern,
                                     const HoleFeature& hole, const GeometryShape& before,
                                     const GeometryShape& seed, const GeometryShape& target,
                                     const std::vector<PatternPlacementTransform>& transforms) {
  const auto fail = [&](std::string message) {
    return Result<GeometryShape>::failure(
        Error::geometry(pattern.value(), "Hole Pattern: " + std::move(message)));
  };
  if (before.empty() || seed.empty() || target.empty())
    return fail("seed/predecessor/target is unavailable");
  try {
    const auto ref = [&](const HolePlacement& placement, HoleReferenceRole role) {
      return HoleInstanceReference::create(hole.id(), placement.id, role).value();
    };
    auto original = HoleInstanceGeometry::resolve_shape(
        part, ref(hole.placements().front(), HoleReferenceRole::Entry), seed);
    auto current = HoleInstanceGeometry::resolve_shape(
        part, ref(hole.placements().front(), HoleReferenceRole::Entry), target);
    if (!original)
      return Result<GeometryShape>::failure(original.error());
    if (!current)
      return Result<GeometryShape>::failure(current.error());
    const double scale = current.value().radius_mm / original.value().radius_mm;
    gp_Trsf scaling, displacement;
    scaling.SetScale(axes(original.value()).Location(), scale);
    displacement.SetDisplacement(axes(original.value()), axes(current.value()));
    const auto mapping = displacement * scaling;
    for (const auto& placement : hole.placements()) {
      auto from =
          HoleInstanceGeometry::resolve_shape(part, ref(placement, HoleReferenceRole::Entry), seed);
      auto to = HoleInstanceGeometry::resolve_shape(part, ref(placement, HoleReferenceRole::Entry),
                                                    target);
      if (!from)
        return Result<GeometryShape>::failure(from.error());
      if (!to)
        return Result<GeometryShape>::failure(to.error());
      auto point = axes(from.value()).Location();
      point.Transform(mapping);
      const auto n = axes(from.value()).Direction().Transformed(mapping);
      if (point.Distance(axes(to.value()).Location()) > 1e-7 ||
          std::abs(from.value().radius_mm * scale - to.value().radius_mm) > 1e-7 ||
          !n.IsEqual(axes(to.value()).Direction(), 1e-7))
        return fail("seed placements no longer share one current rigid/scale frame");
    }
    auto source_lineage = before.impl_->face_lineage;
    source_lineage.insert(source_lineage.end(), seed.impl_->face_lineage.begin(),
                          seed.impl_->face_lineage.end());
    auto removed =
        operation<BRepAlgoAPI_Cut>(pattern, before.impl_->shape, seed.impl_->shape, source_lineage);
    if (!removed || !(volume(removed.value()) > 1e-7))
      return fail("seed removed no reusable material");
    auto seed_tool = std::make_shared<GeometryShape::Impl>(removed.value());
    std::erase_if(source_lineage, [&](const auto& e) {
      return e.origin.feature != hole.id() || !e.origin.role.starts_with("hole/");
    });
    seed_tool->face_lineage = std::move(source_lineage);
    auto mapped = place(GeometryShape(seed_tool), mapping);
    if (!mapped)
      return mapped;
    const double expected = volume(mapped.value().impl_->shape);
    auto result = ResultFaceSupportGeometry::alias_copy(
        target, pattern,
        pattern_source_occurrence_key(PatternSourceReference::feature(hole.id()).value(), {}),
        hole.id());
    for (const auto& occurrence : transforms) {
      // A matching cut volume alone cannot prove an exposed opening: a later
      // boss could cover the seed or a copy while retaining a valid closed void.
      for (const auto& placement : hole.placements()) {
        auto frame = HoleInstanceGeometry::resolve_shape(
            part, ref(placement, HoleReferenceRole::Entry), target);
        if (!frame)
          return Result<GeometryShape>::failure(frame.error());
        auto origin = axes(frame.value()).Location().Transformed(occurrence.transform);
        const auto axis = axes(frame.value()).Direction().Transformed(occurrence.transform);
        auto span = ClosedProfileAdapter{}.projection_span(
            result, {origin.X(), origin.Y(), origin.Z()}, {axis.X(), axis.Y(), axis.Z()});
        if (!span)
          return Result<GeometryShape>::failure(span.error());
        if (span.value().minimum < -1e-7) {
          const double start = span.value().minimum - 1;
          const auto backward =
              BRepPrimAPI_MakeCylinder(gp_Ax2(origin.Translated(gp_Vec(axis) * start), axis),
                                       frame.value().opening_radius_mm, -start)
                  .Shape();
          std::vector<detail::FaceLineage> ignored;
          auto occupied =
              operation<BRepAlgoAPI_Common>(pattern, result.impl_->shape, backward, ignored);
          if (!occupied || std::abs(volume(occupied.value())) > 1e-7 * std::max(1., expected))
            return fail("current entry is covered before the copied support plane");
        }
      }
      if (occurrence.index == PatternOccurrenceIndex{})
        continue;
      auto tool = place(mapped.value(), occurrence.transform);
      if (!tool)
        return tool;
      tool = Result<GeometryShape>::success(ResultFaceSupportGeometry::scope_copy(
          tool.value(), pattern,
          pattern_source_occurrence_key(PatternSourceReference::feature(hole.id()).value(),
                                        occurrence.index)));
      std::vector<detail::FaceLineage> ignored;
      auto material = operation<BRepAlgoAPI_Common>(pattern, result.impl_->shape,
                                                    tool.value().impl_->shape, ignored);
      if (!material ||
          std::abs(volume(material.value()) - expected) > 1e-7 * std::max(1., expected))
        return fail("complete copied cut must fit current material without breakout, duplicate "
                    "cuts or overlap");
      // Verify termination against actual material. Identical Through All cuts
      // may not silently become blind in a locally thicker target. Blind seeds
      // likewise cannot leave new material before the copied entry.
      for (const auto& placement : hole.placements()) {
        auto frame = HoleInstanceGeometry::resolve_shape(
            part, ref(placement, HoleReferenceRole::Axis), target);
        if (!frame)
          return Result<GeometryShape>::failure(frame.error());
        auto origin = axes(frame.value()).Location();
        origin.Transform(occurrence.transform);
        auto axis = axes(frame.value()).Direction().Transformed(occurrence.transform);
        const Point3 start{origin.X(), origin.Y(), origin.Z()};
        const Vector3 direction{axis.X(), axis.Y(), axis.Z()};
        auto span = ClosedProfileAdapter{}.projection_span(result, start, direction);
        if (!span)
          return Result<GeometryShape>::failure(span.error());
        const double end = std::holds_alternative<HoleThroughAllExtent>(hole.extent())
                               ? span.value().maximum + 1
                               : part.find_parameter(std::get<HoleBlindExtent>(hole.extent()).depth)
                                         ->value()
                                         .millimeters() *
                                     scale;
        const auto cylinder =
            BRepPrimAPI_MakeCylinder(gp_Ax2(origin, axis), frame.value().radius_mm, end).Shape();
        auto requested =
            operation<BRepAlgoAPI_Common>(pattern, result.impl_->shape, cylinder, ignored);
        if (!requested)
          return fail("cannot verify current bore termination");
        auto remainder = operation<BRepAlgoAPI_Cut>(pattern, requested.value(),
                                                    tool.value().impl_->shape, ignored);
        if (!remainder || std::abs(volume(remainder.value())) > 1e-7 * std::max(1., expected))
          return fail("copied cut has incompatible current entry or termination; identical recipe "
                      "cannot adapt");
      }
      auto lineage = result.impl_->face_lineage;
      lineage.insert(lineage.end(), tool.value().impl_->face_lineage.begin(),
                     tool.value().impl_->face_lineage.end());
      auto cut = operation<BRepAlgoAPI_Cut>(pattern, result.impl_->shape, tool.value().impl_->shape,
                                            lineage);
      if (!cut || !(volume(cut.value()) > 0))
        return fail("cut must retain a valid positive Body");
      if (std::abs(volume(result.impl_->shape) - volume(cut.value()) - expected) >
          1e-7 * std::max(1., expected))
        return fail("cut changed unexpected material");
      if (hole.thread() && hole.thread()->representation() == ThreadRepresentation::Modeled &&
          largest_tolerance(cut.value()) > 1e-4)
        return fail("modeled result exceeds the 1e-4 mm tolerance cap");
      auto backing = std::make_shared<GeometryShape::Impl>(cut.value());
      backing->face_lineage = std::move(lineage);
      result = GeometryShape(std::move(backing));
    }
    return Result<GeometryShape>::success(std::move(result));
  } catch (const Standard_Failure& e) {
    return fail(standard_failure_message(e, "execution failed"));
  }
}
} // namespace blcad::geometry
