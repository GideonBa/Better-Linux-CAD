#include "blcad/gui/shell/shell_origin.hpp"

#include "blcad/core/datum_axis.hpp"
#include "blcad/core/datum_plane.hpp"

#include <utility>

namespace blcad::gui {

Result<std::size_t> seed_origin_geometry(GuiDocumentSession& session) {
  return session.commit_part_transaction("Ursprung anlegen", [](PartDocument& document) {
    auto xy = DatumPlane::xy();
    auto xz = DatumPlane::xz();
    auto yz = DatumPlane::yz();
    if (xy.has_error())
      return Result<std::size_t>::failure(xy.error());
    if (xz.has_error())
      return Result<std::size_t>::failure(xz.error());
    if (yz.has_error())
      return Result<std::size_t>::failure(yz.error());
    for (auto* plane : {&xy, &xz, &yz}) {
      auto added = document.add_datum_plane(std::move(plane->value()));
      if (added.has_error())
        return Result<std::size_t>::failure(added.error());
    }

    const struct {
      const char* id;
      const char* name;
      Vector3 direction;
    } axes[] = {{"datum.axis.x", "X-Achse", {1.0, 0.0, 0.0}},
                {"datum.axis.y", "Y-Achse", {0.0, 1.0, 0.0}},
                {"datum.axis.z", "Z-Achse", {0.0, 0.0, 1.0}}};
    for (const auto& definition : axes) {
      auto axis = DatumAxis::create_explicit(DatumAxisId(definition.id), definition.name,
                                             Point3{0.0, 0.0, 0.0}, definition.direction);
      if (axis.has_error())
        return Result<std::size_t>::failure(axis.error());
      auto added = document.add_datum_axis(std::move(axis.value()));
      if (added.has_error())
        return Result<std::size_t>::failure(added.error());
    }
    return Result<std::size_t>::success(6U);
  });
}

} // namespace blcad::gui
