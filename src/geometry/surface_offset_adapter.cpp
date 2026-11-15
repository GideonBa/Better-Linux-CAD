#include "blcad/geometry/surface_offset_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include "surface_offset_verification.hpp"
#include <BRepAlgoAPI_Check.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>
#include <cmath>
#include <stdexcept>
namespace blcad::geometry {
namespace {
TopoDS_Face image(BRepOffsetAPI_MakeOffsetShape& operation, const TopoDS_Face& face) {
  TopTools_IndexedMapOfShape images;
  for (TopTools_ListIteratorOfListOfShape it(operation.Generated(face)); it.More(); it.Next())
    if (it.Value().ShapeType() == TopAbs_FACE)
      images.Add(it.Value());
  if (images.Extent() != 1)
    throw std::runtime_error(
        "Offset requires exactly one native offset-skin descendant per source face");
  return TopoDS::Face(images(1));
}
std::string segment(const std::string& value) {
  return std::to_string(value.size()) + ":" + value + "/";
}
std::string skin_role(const detail::FaceLineage& entry, const std::string& side) {
  return "surface/skin/" + side + "/" + segment(entry.origin.feature.value()) +
         segment(entry.origin.role);
}
void checked(const TopoDS_Shape& shape, bool solid) {
  if (shape.IsNull() || !BRepCheck_Analyzer(shape, true).IsValid())
    throw std::runtime_error("Offset/Thicken produced invalid topology; no healing was applied");
  if (!BRepAlgoAPI_Check(shape, false, true).IsValid())
    throw std::runtime_error("Offset skins or side walls collide or self-intersect");
  TopTools_IndexedMapOfShape solids;
  TopExp::MapShapes(shape, TopAbs_SOLID, solids);
  if (solids.Extent() != (solid ? 1 : 0))
    throw std::runtime_error("Thicken requires one Solid; OffsetSurface contains no material");
  if (solid) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    if (!std::isfinite(properties.Mass()) || properties.Mass() <= 1e-12)
      throw std::runtime_error("Thicken requires finite positive material volume");
  }
}
} // namespace
std::optional<SurfaceDistanceReport>
SurfaceOffsetAdapter::verification(const GeometryShape& shape) {
  return shape.empty() ? std::nullopt : shape.impl_->surface_distance;
}
GeometryShape SurfaceOffsetAdapter::with_verification(const GeometryShape& shape,
                                                      SurfaceDistanceReport report) {
  if (shape.empty())
    return shape;
  auto result = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  result->surface_distance = report;
  return GeometryShape(std::move(result));
}
Result<SurfaceConstructionResult>
SurfaceOffsetAdapter::construct(FeatureId id, const GeometryShape& source, SurfaceFeatureKind kind,
                                double distance, SurfaceOffsetSide side, double tolerance,
                                SurfaceOffsetApproximation approximation,
                                std::optional<double> maximum_deviation) const {
  const auto fail = [&](std::string message) {
    return Result<SurfaceConstructionResult>::failure(
        Error::geometry(id.value(), std::move(message)));
  };
  const bool thick = kind == SurfaceFeatureKind::Thicken;
  const bool approximate = approximation == SurfaceOffsetApproximation::VerifiedApproximation;
  if (id.empty() || source.empty() || (!thick && kind != SurfaceFeatureKind::OffsetSurface) ||
      !std::isfinite(distance) || distance < 1e-6 || distance > 1e6 || !std::isfinite(tolerance) ||
      tolerance < 1e-9 || tolerance > 1 || tolerance > distance * .01 ||
      (side != SurfaceOffsetSide::AlongNormal && side != SurfaceOffsetSide::OppositeNormal &&
       side != SurfaceOffsetSide::Symmetric) ||
      (approximation != SurfaceOffsetApproximation::ExactAnalytic && !approximate) ||
      approximate != maximum_deviation.has_value() ||
      (maximum_deviation && (!std::isfinite(*maximum_deviation) || *maximum_deviation < tolerance ||
                             *maximum_deviation > distance * .01)))
    return fail("Offset/Thicken requires supported intent and bounded physical error policy");
  try {
    const auto& original = source.impl_->shape;
    TopTools_IndexedMapOfShape faces, shells, solids;
    TopExp::MapShapes(original, TopAbs_FACE, faces);
    TopExp::MapShapes(original, TopAbs_SHELL, shells);
    TopExp::MapShapes(original, TopAbs_SOLID, solids);
    if (solids.Extent() || faces.Extent() < 1 || faces.Extent() > 64 ||
        (faces.Extent() > 1 && (shells.Extent() != 1 || original.ShapeType() != TopAbs_SHELL)))
      return fail("Offset/Thicken accepts one face or one connected quilt of at most 64 faces");
    checked(original, false);
    const bool symmetric = side == SurfaceOffsetSide::Symmetric;
    const double first = symmetric ? -distance / 2 : 0;
    const double last = symmetric
                            ? distance / 2
                            : (side == SurfaceOffsetSide::OppositeNormal ? -distance : distance);
    const double budget = maximum_deviation.value_or(tolerance);
    detail::validate_offset_source(original, first, last, tolerance, approximate);
    // Native algorithms receive a private deep copy: even tolerance updates cannot mutate input.
    BRepBuilderAPI_Copy copy(original, true, false);
    const auto working = copy.Shape();
    BRepOffsetAPI_MakeOffsetShape lower;
    TopoDS_Shape base = working;
    if (symmetric) {
      lower.PerformBySimple(working, first);
      if (!lower.IsDone())
        return fail("OCCT could not construct the lower symmetric skin");
      base = lower.Shape();
      checked(base, false);
    }
    BRepOffsetAPI_MakeThickSolid thicken;
    BRepOffsetAPI_MakeOffsetShape offset;
    BRepOffsetAPI_MakeOffsetShape* operation;
    TopoDS_Shape candidate;
    if (thick) {
      thicken.MakeThickSolidBySimple(base, symmetric ? distance : last);
      if (!thicken.IsDone())
        return fail("OCCT could not close the offset skins and free side walls");
      candidate = thicken.Shape();
      operation = &thicken;
      if (candidate.ShapeType() != TopAbs_SOLID)
        return fail("Thicken did not form exactly one Solid");
      auto solid = TopoDS::Solid(candidate);
      if (!BRepLib::OrientClosedSolid(solid))
        return fail("Thicken Solid has no consistent outward orientation");
      candidate = solid;
    } else {
      offset.PerformBySimple(symmetric ? working : base, last);
      if (!offset.IsDone())
        return fail("OCCT could not construct the requested offset skin");
      candidate = offset.Shape();
      operation = &offset;
      if (symmetric) {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        builder.Add(compound, base);
        builder.Add(compound, candidate);
        candidate = compound;
      }
    }
    checked(candidate, thick);
    std::vector<detail::OffsetSkinPair> pairs;
    std::vector<detail::FaceLineage> lineage;
    for (int i = 1; i <= faces.Extent(); ++i) {
      const auto original_face = TopoDS::Face(faces(i));
      const auto owned = TopoDS::Face(copy.ModifiedShape(original_face));
      const auto low = symmetric ? image(lower, owned) : owned;
      const auto high = image(*operation, thick ? low : owned);
      pairs.push_back({original_face, low, high, first, last});
      for (const auto& entry : source.impl_->face_lineage)
        if (entry.face.IsSame(original_face)) {
          // Skin identity names authored provenance and side, never a topology ordinal.
          const auto append = [&](const TopoDS_Face& face, const std::string& side) {
            auto next = entry;
            next.origin = {id, skin_role(entry, side)};
            next.face = face;
            next.cosmetic_thread.reset();
            lineage.push_back(std::move(next));
          };
          if (thick || symmetric)
            append(low, symmetric ? "negative" : "source");
          append(high, symmetric ? "positive" : (last < 0 ? "negative" : "positive"));
        }
    }
    const auto criterion = thick ? SurfaceDistanceCriterion::WallThickness
                                 : (symmetric ? SurfaceDistanceCriterion::SkinSeparation
                                              : SurfaceDistanceCriterion::OffsetDistance);
    auto report = detail::verify_offset_skins(pairs, distance, budget, criterion, approximate);
    auto result = std::make_shared<GeometryShape::Impl>(candidate);
    result->surface_distance = report;
    for (auto& entry : lineage)
      detail::append_lineage(result->face_lineage, entry, candidate);
    return Result<SurfaceConstructionResult>::success({GeometryShape(std::move(result)), report});
  } catch (const Standard_Failure& error) {
    return fail(std::string("Offset/Thicken native construction: ") + error.GetMessageString());
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::geometry
