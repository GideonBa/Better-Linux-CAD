#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
TEST_CASE("G04 contour edits reject failed boundary repair atomically and retain exact history",
          "[gui][curved-surface-history][parity-b172]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("g04.history"), "G04"));
  REQUIRE(session.commit_part_transaction("Curved quilt", [](PartDocument& p) {
    p = f::combination();
    return Result<std::size_t>::success(1);
  }));
  const auto saved = [&] { return blcad::test::surface_offset::saved(*session.part_document()); };
  const auto volume = [&] {
    return blcad::test::integrated_solid_volume(
               *session.part_shape_cache()->find_body_shape(BodyId("thick.body")))
        .volume_mm3;
  };
  const auto change = [&](const char* id, double value) {
    return session.commit_part_transaction("Contour correction", [&](PartDocument& p) {
      const auto updated =
          p.set_parameter_value(ParameterId(id), f::checked(Quantity::length_mm(value)));
      if (!updated)
        return Result<std::size_t>::failure(updated.error());
      return Result<std::size_t>::success(updated.value().size());
    });
  };
  const auto original = saved();
  const double area = 20 * std::acos(.8) + 3;
  CHECK(volume() == Catch::Approx(area * 7).margin(1e-5));
  REQUIRE(change("window.height", 6));
  const auto edited = saved();
  CHECK(volume() == Catch::Approx(area * 9).margin(1e-5));
  const auto history_label = std::string(session.undo_label());
  const auto revision = session.presentation_revision();
  // The authored native boundary remains R10. Enlarging its source makes it
  // incompatible; failure must preserve the complete downstream quilt/solid.
  const auto rejected = change("source.diameter", 22);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("boundary") != std::string::npos);
  CHECK(saved() == edited);
  CHECK(session.undo_label() == history_label);
  CHECK(session.presentation_revision() == revision + 1); // The diagnostic refreshes presentation.
  REQUIRE(!session.diagnostics().empty());
  CHECK(session.diagnostics().back().message == rejected.error().message());
  CHECK(volume() == Catch::Approx(area * 9).margin(1e-5));
  REQUIRE(session.undo());
  CHECK(saved() == original);
  CHECK(volume() == Catch::Approx(area * 7).margin(1e-5));
  REQUIRE(session.redo());
  CHECK(saved() == edited);
  CHECK(volume() == Catch::Approx(area * 9).margin(1e-5));
  REQUIRE(change("extension", 4));
  CHECK(volume() == Catch::Approx((20 * std::acos(.8) + 4) * 10).margin(1e-5));
}
