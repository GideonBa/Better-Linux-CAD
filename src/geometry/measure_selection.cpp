#include "blcad/geometry/measure_selection.hpp"
#include "assembly_posed_leaf_shapes.hpp"
#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <memory>
#include <unordered_set>

namespace blcad::geometry {
namespace {
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
Vector3 direction(const gp_Dir& d) {
  return {d.X(), d.Y(), d.Z()};
}
} // namespace
Result<std::vector<MeasureSelectionTarget>>
MeasureSelectionBuilder::build(const std::vector<MeasureSelectionOwner>& owners,
                               std::size_t generation) {
  std::vector<MeasureSelectionTarget> result;
  std::unordered_set<std::string> identities;
  try {
    for (const auto& owner : owners) {
      if (owner.shape.empty() || owner.semantic_id.empty() ||
          !identities.insert(owner.semantic_id).second)
        return Result<std::vector<MeasureSelectionTarget>>::failure(Error::validation(
            "measure.selection", "measurement owners must be nonempty and unique"));
      const auto& shape = owner.shape.impl_->shape;
      if (owner.body)
        result.push_back({owner.semantic_id,
                          owner.label,
                          MeasureSelectionKind::Body,
                          owner.shape,
                          {},
                          {},
                          owner.body});
      const std::string prefix = "measure/" + std::to_string(generation) + "/" +
                                 std::to_string(owner.semantic_id.size()) + ":" +
                                 owner.semantic_id + "/";
      const auto append = [&](const TopoDS_Shape& native, MeasureSelectionKind kind, int index,
                              const char* family, const char* label, Point3 anchor,
                              GeometryMeasureTarget descriptor) {
        result.push_back({prefix + family + "/" + std::to_string(index),
                          owner.label + " / " + label + " " + std::to_string(index), kind,
                          GeometryShape(std::make_shared<GeometryShape::Impl>(native)), anchor,
                          descriptor, owner.body});
      };
      TopTools_IndexedMapOfShape vertices, edges, faces;
      TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);
      TopExp::MapShapes(shape, TopAbs_EDGE, edges);
      TopExp::MapShapes(shape, TopAbs_FACE, faces);
      for (int i = 1; i <= vertices.Extent(); ++i) {
        const Point3 p = point(BRep_Tool::Pnt(TopoDS::Vertex(vertices(i))));
        append(vertices(i), MeasureSelectionKind::Point, i, "vertex", "Point", p, p);
      }
      for (int i = 1; i <= edges.Extent(); ++i) {
        const auto edge = TopoDS::Edge(edges(i));
        if (BRep_Tool::Degenerated(edge))
          continue;
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter(), last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last) || last - first <= 1e-12)
          continue;
        const Point3 anchor = point(curve.Value(first + (last - first) / 2));
        if (curve.GetType() == GeomAbs_Line)
          append(edge, MeasureSelectionKind::Line, i, "edge", "Straight Edge", anchor,
                 AssemblyLineTargetDescriptor{anchor, direction(curve.Line().Direction())});
        else if (curve.GetType() == GeomAbs_Circle) {
          const auto circle = curve.Circle();
          append(edge, MeasureSelectionKind::Circle, i, "edge", "Circular Edge", anchor,
                 AssemblyCircularEdgeTargetDescriptor{
                     point(circle.Location()), direction(circle.XAxis().Direction()),
                     direction(circle.YAxis().Direction()), direction(circle.Axis().Direction()),
                     circle.Radius()});
        }
      }
      for (int i = 1; i <= faces.Extent(); ++i) {
        const auto face = TopoDS::Face(faces(i));
        BRepAdaptor_Surface surface(face);
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        const Point3 anchor = point(properties.CentreOfMass());
        if (surface.GetType() == GeomAbs_Plane) {
          const auto plane = surface.Plane();
          append(face, MeasureSelectionKind::Plane, i, "face", "Planar Face", anchor,
                 AssemblyPlanarTargetDescriptor{anchor, direction(plane.XAxis().Direction()),
                                                direction(plane.YAxis().Direction()),
                                                direction(plane.Axis().Direction())});
        } else if (surface.GetType() == GeomAbs_Cylinder) {
          const auto cylinder = surface.Cylinder();
          append(face, MeasureSelectionKind::Cylinder, i, "face", "Cylindrical Face", anchor,
                 AssemblyCylindricalSurfaceTargetDescriptor{point(cylinder.Location()),
                                                            direction(cylinder.Axis().Direction()),
                                                            cylinder.Radius()});
        }
      }
    }
  } catch (const Standard_Failure& error) {
    return Result<std::vector<MeasureSelectionTarget>>::failure(Error::geometry(
        "measure.selection",
        error.GetMessageString() ? error.GetMessageString() : "OCCT measurement target error"));
  }
  return Result<std::vector<MeasureSelectionTarget>>::success(std::move(result));
}
Result<std::vector<MeasureSelectionTarget>>
MeasureSelectionBuilder::build_project(const Project& project, std::size_t generation) {
  auto posed = detail::build_posed_leaf_shapes(project);
  if (!posed)
    return Result<std::vector<MeasureSelectionTarget>>::failure(posed.error());
  std::vector<MeasureSelectionOwner> owners;
  for (auto& leaf : posed.value().leaves) {
    std::string label;
    const AssemblyDocument* current = &project.assembly();
    for (const auto& id : leaf.leaf.subassembly_path) {
      const auto* occurrence = current ? current->find_subassembly_instance(id) : nullptr;
      if (!occurrence)
        break;
      label += occurrence->name() + " / ";
      current = project.find_assembly_document(occurrence->referenced_assembly_document());
    }
    const auto* assembly = project.find_assembly_document(leaf.leaf.assembly_document);
    const auto* component =
        assembly ? assembly->find_component_instance(leaf.leaf.component_instance) : nullptr;
    label += component ? component->name() : leaf.leaf.component_instance.value();
    owners.push_back({"component/" + detail::leaf_occurrence_key(leaf.leaf),
                      std::move(label),
                      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(leaf.shape))),
                      {}});
  }
  return build(owners, generation);
}
} // namespace blcad::geometry
