#pragma once

#include "blcad/core/result_face_workplane.hpp"
#include "blcad/core/spatial.hpp"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <vector>

namespace blcad::geometry::detail {
struct FaceLineage {
  ResultFaceOrigin origin;
  TopoDS_Face face;
  Point3 frame_origin;
  Vector3 x_axis;
};

inline void append_lineage(std::vector<FaceLineage>& result, FaceLineage entry,
                           const TopoDS_Shape& shape) {
  for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
    if (!faces.Current().IsSame(entry.face))
      continue;
    entry.face = TopoDS::Face(faces.Current());
    if (std::none_of(result.begin(), result.end(), [&](const auto& old) {
          return old.origin == entry.origin && old.face.IsSame(entry.face);
        }))
      result.push_back(std::move(entry));
    return;
  }
}

// Kernel history, rather than geometric proximity, decides which result faces
// descend from an authored face. Keep every descendant so splits stay ambiguous.
template <class Algorithm>
std::vector<FaceLineage> map_lineage(Algorithm& operation, const TopoDS_Shape& result,
                                     const std::vector<FaceLineage>& source) {
  std::vector<FaceLineage> mapped;
  for (const auto& entry : source) {
    append_lineage(mapped, entry, result);
    const auto append = [&](const TopTools_ListOfShape& descendants) {
      for (TopTools_ListIteratorOfListOfShape it(descendants); it.More(); it.Next()) {
        if (it.Value().ShapeType() != TopAbs_FACE)
          continue;
        auto next = entry;
        next.face = TopoDS::Face(it.Value());
        append_lineage(mapped, std::move(next), result);
      }
    };
    append(operation.Modified(entry.face));
    // Generated side walls do not inherit the identity of a support face.
    // Only modified descendants and surviving originals are face continuations.
  }
  return mapped;
}

inline void transform_frames(std::vector<FaceLineage>& entries, const gp_Trsf& transform) {
  for (auto& entry : entries) {
    gp_Pnt point(entry.frame_origin.x, entry.frame_origin.y, entry.frame_origin.z);
    gp_Vec axis(entry.x_axis.x, entry.x_axis.y, entry.x_axis.z);
    point.Transform(transform);
    axis.Transform(transform);
    entry.frame_origin = {point.X(), point.Y(), point.Z()};
    entry.x_axis = {axis.X(), axis.Y(), axis.Z()};
  }
}
} // namespace blcad::geometry::detail
