#pragma once
#include <ChFiDS_Spine.hxx>
#include <Law_Function.hxx>
#include <memory>
#include <string>
namespace blcad::geometry::detail {
// Preserve physical arc-length stations when OCCT approximates an elementary guide with a
// different parameterization. The source snapshot is taken before kernel cap extensions.
Handle(Law_Function)
    fillet_spine_law(const Handle(Law_Function) &, const Handle(ChFiDS_Spine) & source,
                     const Handle(ChFiDS_ElSpine) & guide, std::shared_ptr<std::string> failure);
} // namespace blcad::geometry::detail
