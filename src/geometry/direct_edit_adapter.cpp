#include "blcad/geometry/direct_edit_adapter.hpp"
#include "blcad/core/part_document.hpp"
#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepAdaptor_Curve.hxx>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib_CheckCurveOnSurface.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <GeomTools.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Surface.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Lin.hxx>
#include <gp_Trsf.hxx>
#include <sstream>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace blcad::geometry {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
gp_Vec vector(Vector3 v) {
  return {v.x, v.y, v.z};
}
gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
gp_Vec world(const DirectEditFrame& f, Vector3 v) {
  return vector(f.x_axis) * v.x + vector(f.y_axis) * v.y + vector(cross(f.x_axis, f.y_axis)) * v.z;
}
double value(const PartDocument& p, ParameterId id) {
  const auto* parameter = p.find_parameter(id);
  require(parameter, "Direct Edit parameter is missing");
  return parameter->type() == ParameterType::Angle ? parameter->value().degrees()
                                                   : parameter->value().millimeters();
}
double volume(const TopoDS_Shape& s) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(s, p);
  return p.Mass();
}
void require_native_accuracy(const TopoDS_Shape& shape, double tolerance) {
  // OCCT may conservatively enlarge stored edge/vertex tolerances during
  // intersection even when the actual curves agree to machine precision. Check
  // the complete native pcurve domain and both real endpoints against the
  // authored budget rather than treating those metadata bounds as measured error.
  for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
    const auto face = TopoDS::Face(faces.Current());
    require(BRep_Tool::Tolerance(face) <= tolerance,
            "Direct Edit support approximation exceeds the explicit tolerance budget");
    for (TopExp_Explorer edges(face, TopAbs_EDGE); edges.More(); edges.Next()) {
      const auto edge = TopoDS::Edge(edges.Current());
      if (BRep_Tool::Degenerated(edge)) {
        require(BRep_Tool::Tolerance(edge) <= tolerance,
                "Direct Edit degenerate edge exceeds the explicit tolerance budget");
        for (TopExp_Explorer vertices(edge, TopAbs_VERTEX); vertices.More(); vertices.Next())
          require(BRep_Tool::Tolerance(TopoDS::Vertex(vertices.Current())) <= tolerance,
                  "Direct Edit degenerate vertex exceeds the explicit tolerance budget");
        continue;
      }
      require(BRep_Tool::SameParameter(edge) && BRep_Tool::SameRange(edge),
              "Direct Edit requires matching native edge parameterizations");
      BRepLib_CheckCurveOnSurface check(edge, face);
      check.Perform(false);
      require(check.IsDone() && std::isfinite(check.MaxDistance()) &&
                  check.MaxDistance() <= tolerance,
              "Direct Edit edge/support disagreement exceeds the explicit tolerance budget");
      TopoDS_Vertex start, finish;
      TopExp::Vertices(edge, start, finish);
      require(!start.IsNull() && !finish.IsNull(), "Direct Edit native edge has no endpoints");
      BRepAdaptor_Curve curve(edge);
      require(curve.Value(curve.FirstParameter()).Distance(BRep_Tool::Pnt(start)) <= tolerance &&
                  curve.Value(curve.LastParameter()).Distance(BRep_Tool::Pnt(finish)) <= tolerance,
              "Direct Edit endpoint disagreement exceeds the explicit tolerance budget");
    }
  }
}
bool contains(const std::vector<TopoDS_Face>& faces, const TopoDS_Face& f) {
  return std::any_of(faces.begin(), faces.end(), [&](const auto& a) { return a.IsSame(f); });
}
gp_Vec outward(const TopoDS_Face& face) {
  const auto plane = BRepAdaptor_Surface(face).Plane();
  auto n = gp_Vec(plane.Axis().Direction());
  if ((face.Orientation() == TopAbs_REVERSED) == plane.Position().Direct())
    n.Reverse();
  return n;
}
// Adjacent trims may extend, while the complete underlying support must stay
// identical. Analytic comparison covers every point of the unbounded support;
// other surfaces require exact native definition equality, never sparse samples.
bool same_support(const TopoDS_Face& a, const Handle(Geom_Surface) & b, double tol) {
  BRepAdaptor_Surface first(a);
  GeomAdaptor_Surface second(b);
  if (first.GetType() != second.GetType())
    return false;
  if (first.GetType() == GeomAbs_Plane) {
    const auto x = first.Plane(), y = second.Plane();
    return gp_Vec(x.Axis().Direction()).Crossed(gp_Vec(y.Axis().Direction())).Magnitude() <=
               1e-10 &&
           y.Distance(x.Location()) <= tol;
  }
  if (first.GetType() == GeomAbs_Cylinder) {
    const auto x = first.Cylinder(), y = second.Cylinder();
    return std::abs(x.Radius() - y.Radius()) <= tol &&
           gp_Vec(x.Axis().Direction()).Crossed(gp_Vec(y.Axis().Direction())).Magnitude() <=
               1e-10 &&
           gp_Lin(y.Axis()).Distance(x.Location()) <= tol;
  }
  std::ostringstream left, right;
  GeomTools::Write(BRep_Tool::Surface(a), left);
  GeomTools::Write(b, right);
  return left.str() == right.str();
}

template <class Algorithm>
std::vector<TopoDS_Face> descendants(Algorithm& op, const TopoDS_Shape& result,
                                     const TopoDS_Face& face, bool generated = false) {
  std::vector<TopoDS_Face> out;
  const auto append = [&](const TopoDS_Shape& candidate) {
    if (candidate.ShapeType() != TopAbs_FACE)
      return;
    for (TopExp_Explorer it(result, TopAbs_FACE); it.More(); it.Next())
      if (it.Current().IsSame(candidate) && !contains(out, TopoDS::Face(it.Current())))
        out.push_back(TopoDS::Face(it.Current()));
  };
  append(face);
  for (TopTools_ListIteratorOfListOfShape it(op.Modified(face)); it.More(); it.Next())
    append(it.Value());
  if (generated)
    for (TopTools_ListIteratorOfListOfShape it(op.Generated(face)); it.More(); it.Next())
      append(it.Value());
  return out;
}
struct NativeResult {
  TopoDS_Shape shape;
  std::vector<detail::FaceLineage> lineage;
};
template <class Algorithm>
NativeResult checked(Algorithm& op, const TopoDS_Shape& source,
                     const std::vector<detail::FaceLineage>& lineage,
                     const std::vector<TopoDS_Face>& selected, const DirectEditFeature& f,
                     const std::vector<Handle(Geom_Surface)>& requested, bool erase = false,
                     bool generated = false, bool all_generated = false) {
  const auto result = op.Shape();
  require(!result.IsNull(), "Direct Edit returned an empty result");
  TopTools_IndexedMapOfShape solids;
  TopExp::MapShapes(result, TopAbs_SOLID, solids);
  require(solids.Extent() == 1 && BRepCheck_Analyzer(result, true).IsValid(),
          "Direct Edit must produce one checked solid");
  require(BRepAlgoAPI_Check(result, false, true).IsValid(),
          "Direct Edit produced self-interfering material");
  const double tol = f.policy().tolerance_mm;
  require_native_accuracy(result, tol);
  const double after = volume(result);
  require(std::isfinite(after) && after > tol * tol * tol,
          "Direct Edit produced no positive finite material");
  if (erase)
    require(std::abs(after - volume(source)) > tol * tol * tol,
            "Direct Edit healing did not change material");
  std::vector<TopoDS_Face> accounted;
  for (TopExp_Explorer it(source, TopAbs_FACE); it.More(); it.Next()) {
    const auto original = TopoDS::Face(it.Current());
    const auto next = descendants(op, result, original,
                                  all_generated || (contains(selected, original) && generated));
    if (contains(selected, original)) {
      const auto index =
          static_cast<std::size_t>(std::find_if(selected.begin(), selected.end(),
                                                [&](const auto& a) { return a.IsSame(original); }) -
                                   selected.begin());
      if (erase) {
        require(next.empty(), "Direct Edit healing left a selected face behind");
        continue;
      }
      require(next.size() == 1, "Direct Edit selected face continuation is missing or ambiguous");
      require(same_support(next.front(), requested[index], tol),
              "Direct Edit result does not satisfy the requested face support");
    } else {
      require(next.size() == 1,
              "Direct Edit unselected support continuation is missing, split or ambiguous");
      for (const auto& face : next) {
        require(
            same_support(face, BRep_Tool::Surface(original), tol),
            "Direct Edit changed an unselected support; include or remove its blend explicitly");
        const BRepAdaptor_Surface old_support(original), new_support(face);
        if (old_support.GetType() == GeomAbs_Plane)
          require(outward(original).Dot(outward(face)) >= 1 - 1e-10,
                  "Direct Edit inverted an unselected material half-space");
        else if (old_support.GetType() == GeomAbs_Cylinder)
          require(((original.Orientation() == TopAbs_REVERSED) !=
                   old_support.Cylinder().Position().Direct()) ==
                      ((face.Orientation() == TopAbs_REVERSED) !=
                       new_support.Cylinder().Position().Direct()),
                  "Direct Edit inverted an unselected cylinder material side");
        else
          require(original.Orientation() == face.Orientation(),
                  "Direct Edit inverted an unselected support orientation");
      }
    }
    accounted.insert(accounted.end(), next.begin(), next.end());
  }
  for (TopExp_Explorer it(result, TopAbs_FACE); it.More(); it.Next())
    require(contains(accounted, TopoDS::Face(it.Current())),
            "Direct Edit created an unrequested support");
  NativeResult out{result, detail::map_lineage(op, result, lineage)};
  // Offset and Draft report selected continuations as Generated on some OCCT versions.
  for (const auto& entry : lineage)
    if (all_generated || (contains(selected, entry.face) && generated))
      for (const auto& face : descendants(op, result, entry.face, true)) {
        auto next = entry;
        next.face = face;
        detail::append_lineage(out.lineage, std::move(next), result);
      }
  if (!erase)
    for (std::size_t i = 0; i < selected.size(); ++i)
      for (const auto& face : descendants(op, result, selected[i], generated))
        detail::append_lineage(
            out.lineage,
            {{f.id(), "face/" + std::to_string(i)}, face, f.frame().origin_mm, f.frame().x_axis},
            result);
  return out;
}
} // namespace

Result<GeometryShape> DirectEditAdapter::apply(const PartDocument& p, const DirectEditFeature& f,
                                               const GeometryShape& target, const ShapeCache& cache,
                                               StepImportTopologyContext* imports) const {
  try {
    require(!target.empty(), "Direct Edit target Body is missing");
    require_native_accuracy(target.impl_->shape, f.policy().tolerance_mm);
    if (auto valid = validate_direct_edit_parameters(p, f); !valid)
      return Result<GeometryShape>::failure(valid.error());
    // Every native algorithm works on a private copy; even failed OCCT recovery
    // cannot modify the immutable imported base or a certified cached result.
    std::vector<TopoDS_Face> original;
    for (const auto& selection : f.selections()) {
      auto resolved = resolve_semantic_face(p, cache, target.impl_->shape, selection.reference,
                                            f.id(), "Direct Edit", imports);
      if (!resolved)
        return Result<GeometryShape>::failure(resolved.error());
      require(!contains(original, resolved.value()), "Direct Edit resolves duplicate faces");
      const auto type = BRepAdaptor_Surface(resolved.value()).GetType();
      require(
          selection.expected == DirectEditFaceCapability::AnySurface ||
              (selection.expected == DirectEditFaceCapability::Planar && type == GeomAbs_Plane) ||
              (selection.expected == DirectEditFaceCapability::Cylindrical &&
               type == GeomAbs_Cylinder),
          "Direct Edit face no longer has its expected capability");
      original.push_back(resolved.value());
    }
    BRepBuilderAPI_Copy copy(target.impl_->shape, true, false);
    const auto source = copy.Shape();
    const auto lineage = detail::map_lineage(copy, source, target.impl_->face_lineage);
    std::vector<TopoDS_Face> faces;
    for (const auto& face : original) {
      const auto next = descendants(copy, source, face);
      require(next.size() == 1, "Cannot copy Direct Edit face uniquely");
      faces.push_back(next.front());
    }
    std::vector<Handle(Geom_Surface)> requested;
    NativeResult native;
    if (std::holds_alternative<DirectFaceDeleteHeal>(f.operation())) {
      BRepAlgoAPI_Defeaturing heal;
      heal.SetShape(source);
      heal.SetToFillHistory(true);
      for (const auto& face : faces)
        heal.AddFaceToRemove(face);
      heal.Build();
      require(heal.IsDone() && !heal.HasErrors() && !heal.HasWarnings(),
              "Direct Edit cannot heal every selected face; candidate was rejected");
      native = checked(heal, source, lineage, faces, f, requested, true);
    } else if (const auto* rotate = std::get_if<DirectFaceRotate>(&f.operation())) {
      const gp_Vec axis = world(f.frame(), rotate->axis).Normalized();
      const gp_Pnt origin = point(f.frame().origin_mm);
      const double angle = value(p, rotate->angle) * std::numbers::pi / 180;
      require(std::abs(angle) > 1e-10 && std::abs(angle) < std::numbers::pi / 2,
              "Direct Edit planar rotation requires a nonzero angle below 90 degrees");
      gp_Trsf rotation;
      rotation.SetRotation(gp_Ax1(origin, gp_Dir(axis)), angle);
      BRepOffsetAPI_DraftAngle draft(source);
      for (const auto& face : faces) {
        BRepAdaptor_Surface surface(face);
        require(surface.GetType() == GeomAbs_Plane,
                "Direct Edit rotation currently requires planar faces");
        const auto n = outward(face);
        require(std::abs(n.Dot(axis)) < 1e-9 &&
                    surface.Plane().Distance(origin) <= f.policy().tolerance_mm,
                "Direct Edit rotation axis must lie on every selected plane");
        const auto neutral = axis.Crossed(n).Normalized();
        draft.Add(face, gp_Dir(neutral), angle, gp_Pln(origin, gp_Dir(neutral)), true);
        require(draft.AddDone(), "Direct Edit rotation cannot extend the adjacent supports");
        requested.push_back(
            Handle(Geom_Surface)::DownCast(BRep_Tool::Surface(face)->Transformed(rotation)));
      }
      draft.Build();
      require(draft.IsDone(), "Direct Edit rotation failed to rebuild adjacent faces");
      native = checked(draft, source, lineage, faces, f, requested, false, true);
    } else {
      BRepOffset_MakeOffset offset;
      offset.Initialize(source, 0, f.policy().tolerance_mm, BRepOffset_Skin, true, false,
                        GeomAbs_Intersection);
      for (const auto& face : faces) {
        BRepAdaptor_Surface surface(face);
        double distance = 0;
        auto support = Handle(Geom_Surface)::DownCast(BRep_Tool::Surface(face)->Copy());
        if (const auto* move = std::get_if<DirectFaceMove>(&f.operation())) {
          require(surface.GetType() == GeomAbs_Plane,
                  "Direct Edit Move currently requires planar faces");
          const auto d = world(f.frame(), {value(p, move->x) * move->signs.x,
                                           value(p, move->y) * move->signs.y,
                                           value(p, move->z) * move->signs.z});
          distance = outward(face).Dot(d);
          support->Translate(d);
        } else if (const auto* size = std::get_if<DirectFaceSize>(&f.operation())) {
          require(surface.GetType() == GeomAbs_Cylinder,
                  "Direct Edit Size requires a cylindrical face");
          const double radius =
              value(p, size->size) / (size->mode == DirectEditSizeMode::Diameter ? 2 : 1);
          require(radius > f.policy().tolerance_mm,
                  "Direct Edit radius is below modeling tolerance");
          const auto cylinder = surface.Cylinder();
          const double delta = radius - cylinder.Radius();
          const bool radial_outward =
              (face.Orientation() == TopAbs_REVERSED) != cylinder.Position().Direct();
          distance = radial_outward ? delta : -delta;
          auto resized = cylinder;
          resized.SetRadius(radius);
          support = new Geom_CylindricalSurface(resized);
        } else {
          const auto& intent = std::get<DirectFaceOffset>(f.operation());
          distance = value(p, intent.distance) * (intent.reverse ? -1 : 1);
          require(surface.GetType() == GeomAbs_Plane || surface.GetType() == GeomAbs_Cylinder,
                  "Direct Edit Offset supports analytic planar and cylindrical faces");
          if (surface.GetType() == GeomAbs_Plane)
            support->Translate(outward(face) * distance);
          else {
            auto cylinder = surface.Cylinder();
            const bool radial_outward =
                (face.Orientation() == TopAbs_REVERSED) != cylinder.Position().Direct();
            const auto radius = cylinder.Radius() + (radial_outward ? distance : -distance);
            require(radius > f.policy().tolerance_mm, "Direct Edit Offset collapses a cylinder");
            cylinder.SetRadius(radius);
            support = new Geom_CylindricalSurface(cylinder);
          }
        }
        require(std::abs(distance) > f.policy().tolerance_mm,
                "Direct Edit has no normal face displacement");
        requested.push_back(support);
        offset.SetOffsetOnFace(face, distance);
      }
      offset.MakeOffsetShape();
      require(offset.IsDone(), "Direct Edit offset/healing failed to reconstruct a solid");
      native = checked(offset, source, lineage, faces, f, requested, false, true, true);
    }
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(native.shape));
    backing->face_lineage = std::move(native.lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(Error::geometry(
        f.id().value(),
        e.GetMessageString() ? e.GetMessageString() : "Direct Edit native operation failed"));
  } catch (const std::exception& e) {
    return Result<GeometryShape>::failure(Error::geometry(f.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
