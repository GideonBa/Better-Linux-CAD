#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/geometry/surface_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>
namespace blcad::geometry {
GeometryShape SurfaceSelectionGeometry::tag_single_patch(const GeometryShape& shape,
                                                         FeatureId feature, std::string role) {
  if (shape.empty())
    return shape;
  TopTools_IndexedMapOfShape faces;
  TopExp::MapShapes(shape.impl_->shape, TopAbs_FACE, faces);
  if (faces.Extent() != 1)
    return shape;
  const auto face = TopoDS::Face(faces(1));
  double u0, u1, v0, v1;
  BRepTools::UVBounds(face, u0, u1, v0, v1);
  BRepAdaptor_Surface surface(face);
  gp_Pnt point;
  gp_Vec du, dv;
  surface.D1((u0 + u1) / 2, (v0 + v1) / 2, point, du, dv);
  if (du.SquareMagnitude() < 1e-24)
    return shape;
  du.Normalize();
  auto result = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  detail::append_lineage(result->face_lineage,
                         {{std::move(feature), std::move(role)},
                          face,
                          {point.X(), point.Y(), point.Z()},
                          {du.X(), du.Y(), du.Z()},
                          true},
                         result->shape);
  return GeometryShape(std::move(result));
}
Result<GeometryShape> SurfaceSelectionGeometry::resolve(const PartDocument& part,
                                                        const ShapeCache& cache,
                                                        const SurfaceReference& reference,
                                                        FeatureId consumer, double tolerance,
                                                        StepImportTopologyContext* imports,
                                                        const GeometryShape* owned_override) {
  const auto fail = [&](std::string message) {
    return Result<GeometryShape>::failure(Error::geometry(consumer.value(), std::move(message)));
  };
  if (!std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1)
    return fail("Surface selection requires a bounded physical tolerance");
  try {
    if (reference.source_kind() == SurfaceReferenceSourceKind::Body) {
      const auto* shape = owned_override
                              ? owned_override
                              : cache.find_body_shape(std::get<BodyId>(reference.source()));
      if (!shape || shape->empty())
        return fail("Surface quilt has no current geometry");
      if (TopExp_Explorer(shape->impl_->shape, TopAbs_SOLID).More())
        return fail("Select explicit faces of a Solid; a quilt input contains no material");
      return Result<GeometryShape>::success(*shape);
    }
    if (reference.source_kind() == SurfaceReferenceSourceKind::SemanticFace) {
      const auto& semantic = std::get<SemanticFaceReference>(reference.source());
      auto face = SurfaceAdapter{}.extract_semantic_face(consumer, part, cache, semantic);
      if (!face)
        return face;
      const auto* producer = cache.find_feature_shape(semantic.source_feature());
      if (producer) {
        auto backing = std::make_shared<GeometryShape::Impl>(*face.value().impl_);
        for (const auto& entry : producer->impl_->face_lineage)
          detail::append_lineage(backing->face_lineage, entry, backing->shape);
        if (!backing->face_lineage.empty())
          return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
      }
      return Result<GeometryShape>::success(
          tag_single_patch(face.value(), semantic.source_feature(),
                           "surface/semantic/" + std::string(to_string(semantic.face()))));
    }
    const auto& selection = std::get<SurfaceFaceSelection>(reference.source());
    const auto* owned = owned_override ? owned_override : cache.find_body_shape(selection.body);
    if (!owned || owned->empty())
      return fail("Selected Surface owner has no effective geometry");
    std::vector<TopoDS_Face> selected;
    std::vector<detail::FaceLineage> lineage;
    for (const auto& reference : selection.faces) {
      TopoDS_Face face;
      if (const auto* result = std::get_if<ResultFaceReference>(&reference.source())) {
        std::vector<TopoDS_Face> matches;
        for (const auto& origin : result->origins()) {
          bool found = false;
          for (const auto& entry : owned->impl_->face_lineage)
            if (entry.origin == origin) {
              found = true;
              if (std::none_of(matches.begin(), matches.end(),
                               [&](const auto& old) { return old.IsSame(entry.face); }))
                matches.push_back(entry.face);
            }
          if (!found)
            return fail("Lost selected face: an authored contributor was deleted");
        }
        if (matches.size() != 1)
          return fail("Ambiguous selected face: authored contributors split");
        face = matches.front();
      } else {
        auto resolved = resolve_semantic_face(part, cache, owned->impl_->shape, reference, consumer,
                                              "Surface", imports);
        if (!resolved)
          return Result<GeometryShape>::failure(resolved.error());
        face = resolved.value();
      }
      if (std::any_of(selected.begin(), selected.end(),
                      [&](const auto& old) { return old.IsSame(face); }))
        return fail("Distinct Surface references resolve to the same face");
      selected.push_back(face);
      const auto before = lineage.size();
      for (const auto& entry : owned->impl_->face_lineage)
        if (entry.face.IsSame(face))
          lineage.push_back(entry);
      if (lineage.size() == before) {
        const auto* imported = std::get_if<ImportedTopologyReference>(&reference.source());
        if (imported) {
          std::vector<FeatureId> producers;
          for (const auto& base : part.imported_body_features())
            if (base.definition().catalog() == imported->catalog())
              for (const auto& output : base.definition().outputs())
                if (output.body == selection.body)
                  producers.push_back(base.id());
          if (producers.size() != 1)
            return fail("Imported Surface face has ambiguous base provenance");
          const auto segment = [](const std::string& value) {
            return std::to_string(value.size()) + ":" + value + "/";
          };
          const auto patch = tag_single_patch(
              GeometryShape(std::make_shared<GeometryShape::Impl>(face)), producers.front(),
              "surface/input/imported/" + segment(imported->catalog().value()) +
                  segment(imported->entry().value()));
          lineage.insert(lineage.end(), patch.impl_->face_lineage.begin(),
                         patch.impl_->face_lineage.end());
        }
      }
    }
    TopoDS_Shape shape;
    if (selected.size() == 1)
      shape = selected.front();
    else {
      BRep_Builder builder;
      TopoDS_Compound compound;
      builder.MakeCompound(compound);
      for (const auto& face : selected)
        builder.Add(compound, face);
      BRepBuilderAPI_Copy copy(compound, true, false);
      BRepBuilderAPI_Sewing sewing(tolerance);
      for (const auto& face : selected) {
        auto owned = copy.ModifiedShape(face);
        owned.Orientation(face.Orientation());
        sewing.Add(owned);
      }
      for (auto& entry : lineage)
        entry.face = TopoDS::Face(copy.ModifiedShape(entry.face));
      sewing.Perform();
      shape = sewing.SewedShape();
      if (shape.IsNull() || sewing.NbMultipleEdges() > 0)
        return fail("Surface selection is disconnected or non-manifold");
      TopTools_IndexedMapOfShape faces, shells;
      TopExp::MapShapes(shape, TopAbs_FACE, faces);
      TopExp::MapShapes(shape, TopAbs_SHELL, shells);
      if (shells.Extent() != 1 || faces.Extent() != int(selected.size()))
        return fail("Select one connected quilt within the authored tolerance");
      shape = shells(1);
      for (auto& entry : lineage) {
        const auto image = sewing.ModifiedSubShape(entry.face);
        if (!image.IsNull() && image.ShapeType() == TopAbs_FACE)
          entry.face = TopoDS::Face(image);
      }
    }
    if (shape.IsNull() || !BRepCheck_Analyzer(shape, true).IsValid())
      return fail("Selected quilt is topologically invalid");
    auto result = std::make_shared<GeometryShape::Impl>(shape);
    for (auto& entry : lineage)
      detail::append_lineage(result->face_lineage, entry, shape);
    return Result<GeometryShape>::success(GeometryShape(std::move(result)));
  } catch (const Standard_Failure& error) {
    return fail(std::string("Surface selection: ") + error.GetMessageString());
  }
}
} // namespace blcad::geometry
