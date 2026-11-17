#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/sketch_driven_adapter.hpp"
#include "emboss_native.hpp"
#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <stdexcept>
namespace blcad::geometry {
namespace {
template <class T> T take(Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().object_id() + ": " + r.error().message());
  return std::move(r.value());
}
void require(bool yes, const char* message) {
  if (!yes)
    throw std::runtime_error(message);
}
int solids(const TopoDS_Shape& shape) {
  int count = 0;
  for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next())
    ++count;
  return count;
}
template <class Algorithm>
detail::RibNative combine(const detail::RibNative& target, const detail::RibNative& tool) {
  Algorithm op;
  TopTools_ListOfShape args, tools;
  args.Append(target.shape);
  tools.Append(tool.shape);
  op.SetArguments(args);
  op.SetTools(tools);
  op.SetNonDestructive(true);
  op.Build();
  require(op.IsDone() && !op.HasErrors(), "Emboss/Engrave material operation failed");
  detail::RibNative out{op.Shape(), detail::map_lineage(op, op.Shape(), target.lineage)};
  auto added = detail::map_lineage(op, op.Shape(), tool.lineage);
  out.lineage.insert(out.lineage.end(), added.begin(), added.end());
  return out;
}
} // namespace
Result<GeometryShape> SketchDrivenAdapter::apply_emboss(const PartDocument& p,
                                                        const SketchDrivenFeature& feature,
                                                        const GeometryShape& target,
                                                        const ShapeCache& cache,
                                                        StepImportTopologyContext* imports) const {
  try {
    const auto& e = std::get<EmbossIntent>(feature.definition().intent);
    const auto* sketch = p.find_sketch(emboss_source_sketch(e));
    require(sketch, "Emboss source Sketch is missing");
    const auto frame = take(WorkplaneResolver{}.resolve_for_sketch(p, *sketch, cache));
    const auto* depth = p.find_parameter(e.depth);
    require(depth && depth->type() == ParameterType::Length && depth->value().millimeters() > 1e-6,
            "Emboss depth must exceed numerical tolerance");
    double angle = 0;
    if (e.orientation) {
      const auto* v = p.find_parameter(*e.orientation);
      require(v && v->type() == ParameterType::Angle, "Emboss orientation binding is missing");
      angle = v->value().degrees();
    }
    std::vector<TopoDS_Face> supports;
    for (const auto& ref : e.supports) {
      auto face = take(resolve_semantic_face(p, cache, target.impl_->shape, ref, feature.id(),
                                             "Emboss", imports));
      require(std::none_of(supports.begin(), supports.end(),
                           [&](const auto& old) { return old.IsSame(face); }),
              "Emboss selected the same physical support more than once");
      supports.push_back(face);
    }
    auto outlines =
        std::holds_alternative<ProfileRegionReference>(e.source)
            ? detail::emboss_profile_outlines(p, std::get<ProfileRegionReference>(e.source), frame)
            : detail::emboss_text_outlines(p, std::get<EmbossTextSource>(e.source));
    auto chart = detail::emboss_chart(e, supports, frame, depth->value().millimeters(), angle);
    detail::map_emboss_outlines(outlines, chart);
    auto tool = detail::emboss_tools(outlines, chart, supports, feature.id());
    BRepBuilderAPI_Copy copy(target.impl_->shape, true, false);
    detail::RibNative source{copy.Shape(),
                             detail::map_lineage(copy, copy.Shape(), target.impl_->face_lineage)};
    auto result = e.operation == EmbossOperation::Raise ? combine<BRepAlgoAPI_Fuse>(source, tool)
                                                        : combine<BRepAlgoAPI_Cut>(source, tool);
    detail::validate_rib_solid(result.shape);
    require(solids(result.shape) == solids(target.impl_->shape),
            "Emboss/Engrave must preserve a connected target Body");
    const double before = detail::rib_volume(target.impl_->shape),
                 after = detail::rib_volume(result.shape);
    require(after > 1e-8 && (e.operation == EmbossOperation::Raise ? after > before + 1e-7
                                                                   : after < before - 1e-7),
            "Emboss/Engrave has no positive material change or erases its target");
    auto witnesses = source.lineage;
    witnesses.insert(witnesses.end(), tool.lineage.begin(), tool.lineage.end());
    detail::scope_rib_fragments(result, witnesses, feature.id(), "emboss/fragment/");
    auto backing = std::make_shared<GeometryShape::Impl>(result.shape);
    backing->face_lineage = std::move(result.lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(Error::geometry(
        feature.id().value(),
        e.GetMessageString() ? e.GetMessageString() : "Native Emboss/Engrave failed"));
  } catch (const std::exception& e) {
    return Result<GeometryShape>::failure(Error::geometry(feature.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
