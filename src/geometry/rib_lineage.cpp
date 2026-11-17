#include "rib_native.hpp"
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <map>
#include <set>
namespace blcad::geometry::detail {
void scope_rib_fragments(RibNative& result, const std::vector<FaceLineage>& witnesses,
                         const FeatureId& feature) {
  struct Candidate {
    TopoDS_Face face;
    std::vector<FaceLineage> origins;
    std::string signature;
  };
  std::vector<Candidate> candidates;
  for (const auto& item : result.lineage) {
    auto found = std::find_if(candidates.begin(), candidates.end(),
                              [&](const auto& c) { return c.face.IsSame(item.face); });
    if (found == candidates.end())
      candidates.push_back({item.face, {item}, {}});
    else
      found->origins.push_back(item);
  }
  std::map<std::string, int> counts, origin_counts;
  const auto segment = [](const std::string& s) {
    return std::to_string(s.size()) + ":" + s + "/";
  };
  for (auto& c : candidates) {
    std::set<std::string> keys;
    for (const auto& item : c.origins)
      keys.insert(segment(item.origin.feature.value()) + segment(item.origin.role));
    for (const auto& key : keys) {
      c.signature += key;
      ++origin_counts[key];
    }
    ++counts[c.signature];
  }
  for (const auto& c : candidates) {
    bool unique = false;
    for (auto& entry : result.lineage)
      if (entry.face.IsSame(c.face)) {
        const auto key = segment(entry.origin.feature.value()) + segment(entry.origin.role);
        if (origin_counts[key] > 1)
          entry.selectable = false;
        else
          unique |= entry.selectable;
      }
    if (unique)
      continue;
    Bnd_Box bounds;
    BRepBndLib::Add(c.face, bounds);
    std::set<std::string> contacts;
    for (const auto& witness : witnesses) {
      Bnd_Box other;
      BRepBndLib::Add(witness.face, other);
      if (bounds.IsOut(other))
        continue;
      BRepAlgoAPI_Common common;
      TopTools_ListOfShape args, tools;
      args.Append(c.face);
      tools.Append(witness.face);
      common.SetArguments(args);
      common.SetTools(tools);
      common.SetNonDestructive(true);
      common.Build();
      if (!common.IsDone() || common.HasErrors())
        continue;
      GProp_GProps area, edges;
      BRepGProp::SurfaceProperties(common.Shape(), area);
      if (std::abs(area.Mass()) > 1e-8)
        continue;
      BRepAlgoAPI_Section section(c.face, witness.face, false);
      section.SetNonDestructive(true);
      section.Build();
      if (!section.IsDone() || section.HasErrors())
        continue;
      BRepGProp::LinearProperties(section.Shape(), edges);
      if (edges.Mass() > 1e-6)
        contacts.insert(segment(witness.origin.feature.value()) + segment(witness.origin.role));
    }
    if (contacts.empty() && counts[c.signature] > 1)
      continue;
    std::string scope = segment(c.signature);
    for (const auto& key : contacts)
      scope += segment(key);
    auto entry = c.origins.front();
    entry.origin = {feature, "rib/fragment/" + scope};
    entry.selectable = true;
    append_lineage(result.lineage, std::move(entry), result.shape);
  }
}
} // namespace blcad::geometry::detail
