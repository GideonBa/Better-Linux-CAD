#include "blcad/core/interactive_sketcher_acceptance.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

using namespace blcad;

TEST_CASE("Block 121 coverage manifest contains every planned Sketch capability",
          "[integration][interactive-sketcher][integration][sketch-gui-headless]") {
  constexpr auto coverage = InteractiveSketcherAcceptance::coverage();
  REQUIRE(coverage.size() == 15U);

  for (std::size_t i = 0; i < coverage.size(); ++i) {
    INFO("capability: " << coverage[i].capability);
    CHECK_FALSE(coverage[i].capability.empty());
    CHECK_FALSE(coverage[i].contract.empty());
    CHECK_FALSE(coverage[i].focused_tag.empty());
    CHECK(InteractiveSketcherAcceptance::covers_all_evidence(coverage[i].evidence));

    for (std::size_t j = i + 1U; j < coverage.size(); ++j)
      CHECK(coverage[i].capability != coverage[j].capability);
  }
}

TEST_CASE("Block 121 acceptance evidence freezes all required interaction invariants",
          "[integration][interactive-sketcher]") {
  constexpr unsigned expected =
      static_cast<unsigned>(SketchAcceptanceEvidence::MouseScriptEquivalence) |
      static_cast<unsigned>(SketchAcceptanceEvidence::PersistenceRecompute) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ExactUndoRedo) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ConflictAtomicity) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ReferenceRepair) |
      static_cast<unsigned>(SketchAcceptanceEvidence::KeyboardApplyCancel) |
      static_cast<unsigned>(SketchAcceptanceEvidence::HighDpiMapping) |
      static_cast<unsigned>(SketchAcceptanceEvidence::NoStalePreview);
  CHECK(InteractiveSketcherAcceptance::all_evidence == expected);
}

TEST_CASE("Block 121 performance budgets are deterministic and bounded",
          "[performance][sketch-interaction]") {
  constexpr auto budgets = InteractiveSketcherAcceptance::performance_budgets();
  REQUIRE(budgets.size() == 4U);
  CHECK(budgets[0].operation == std::string_view("hover-hit-test"));
  CHECK(budgets[1].operation == std::string_view("solver-backed-drag-frame"));
  CHECK(budgets[2].operation == std::string_view("planar-solve"));
  CHECK(budgets[3].operation == std::string_view("region-recognition"));

  for (const auto& budget : budgets) {
    CHECK(budget.representative_entity_count > 0U);
    CHECK(budget.budget_milliseconds > 0.0);
    CHECK(budget.budget_milliseconds <= 50.0);
  }
}
