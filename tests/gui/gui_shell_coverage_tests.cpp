#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QToolButton>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_registry_hub.hpp>
#include <catch2/interfaces/catch_interfaces_testcase.hpp>
#include <catch2/internal/catch_test_spec_parser.hpp>

using namespace blcad::gui;
namespace {
const QString root = QStringLiteral(BLCAD_SOURCE_DIR) + '/';
QJsonObject read_json(const QString& relative) {
  QFile file(root + relative);
  REQUIRE(file.open(QIODevice::ReadOnly));
  QJsonParseError error;
  auto document = QJsonDocument::fromJson(file.readAll(), &error);
  REQUIRE(error.error == QJsonParseError::NoError);
  REQUIRE(document.isObject());
  return document.object();
}
bool repository_file(const QString& relative) {
  const auto canonical = QFileInfo(root + relative).canonicalFilePath();
  return !relative.isEmpty() && !QFileInfo(relative).isAbsolute() &&
         QFileInfo(root + relative).isFile() && canonical.startsWith(root);
}
// Inventory validation deliberately does not execute the referenced tests or
// equate tag existence with interaction correctness. Their suites run separately.
QStringList audit(const QJsonObject& manifest, const QJsonObject& baseline) {
  QStringList errors;
  const auto require = [&](bool condition, const QString& message) {
    if (!condition)
      errors << message;
  };
  require(manifest["schema"] == "blcad.gui-feature-coverage.v3", "schema");
  require(manifest["implemented_through"].toInt() == 131, "historical boundary");
  require(repository_file(manifest["extends"].toString()), "baseline path");
  require(repository_file(manifest["acceptance_contract"].toString()), "historical contract");
  require(repository_file(manifest["current_shell_acceptance_contract"].toString()),
          "shell contract");
  QSet<QString> expected;
  for (auto item : baseline["families"].toArray()) {
    const auto name = item.toObject()["family"].toString();
    require(!expected.contains(name) && !name.isEmpty(), "baseline family uniqueness");
    expected.insert(name);
  }
  require(expected.size() == 18, "accepted family boundary");
  QSet<QString> found, blocked;
  const auto& registry = Catch::getRegistryHub();
  for (auto entry : manifest["families"].toArray()) {
    const auto family = entry.toObject();
    const auto name = family["family"].toString();
    require(!name.isEmpty() && !found.contains(name), "duplicate/empty family: " + name);
    found.insert(name);
    for (auto accepted : baseline["families"].toArray())
      if (accepted.toObject()["family"] == name)
        require(family["validation_owner"] == accepted.toObject()["owner"],
                "validation owner: " + name);
    const auto historical = family["interactive"].toObject();
    if (historical["disposition"] == "interactive") {
      require(historical["owner"].toInt() >= 106 && historical["owner"].toInt() <= 131,
              "historical owner: " + name);
      require(repository_file(historical["contract"].toString()), "historical contract: " + name);
    } else {
      require(historical["disposition"] == "validation_sufficient" &&
                  !historical["justification"].toString().trimmed().isEmpty(),
              "historical disposition: " + name);
    }
    const auto shell = family["shell"].toObject();
    require(shell["owner"].toInt() == 136, "shell owner: " + name);
    require(repository_file(shell["contract"].toString()), "shell contract: " + name);
    require(!shell["scope"].toString().trimmed().isEmpty(), "shell scope: " + name);
    require(shell["gaps"].isArray(), "gaps inventory: " + name);
    const bool partial = !shell["gaps"].toArray().isEmpty();
    require(shell["status"] == (partial ? "partial" : "restored"), "scope status: " + name);
    for (auto gap : shell["gaps"].toArray())
      require(!gap.toString().trimmed().isEmpty(), "empty gap: " + name);
    if (partial)
      blocked.insert(name);
    require(shell["proofs"].isArray() && !shell["proofs"].toArray().isEmpty(),
            "missing proof: " + name);
    for (auto item : shell["proofs"].toArray()) {
      const auto proof = item.toObject();
      const auto filter = proof["filter"].toString();
      const auto source = proof["source"].toString();
      // Each proof is one positive AND filter. Comma-OR alternatives must be
      // separate entries, so one surviving arm cannot conceal a deleted tag.
      require(QRegularExpression(R"(^(\[[A-Za-z0-9_-]+\])+$)").match(filter).hasMatch(),
              "single positive filter: " + filter);
      require(source.startsWith("tests/gui/gui_shell_") && repository_file(source),
              "shell proof source: " + source);
      const auto spec = Catch::TestSpecParser(registry.getTagAliasRegistry())
                            .parse(filter.toStdString())
                            .testSpec();
      require(spec.hasFilters() && spec.getInvalidSpecs().empty(), "invalid filter: " + filter);
      std::size_t matched = 0;
      for (const auto* test : registry.getTestCaseRegistry().getAllInfos()) {
        if (!spec.matches(*test))
          continue;
        ++matched;
        require(!test->isHidden(), "hidden proof: " + filter);
        require(QFileInfo(QString::fromUtf8(test->lineInfo.file)).canonicalFilePath() ==
                    QFileInfo(root + source).canonicalFilePath(),
                "proof source mismatch: " + filter);
      }
      require(matched > 0, "unresolved proof: " + filter);
    }
    require(shell["actions"].isArray() && shell["widgets"].isArray() &&
                (!shell["actions"].toArray().isEmpty() || !shell["widgets"].toArray().isEmpty()),
            "missing shell entry point: " + name);
  }
  require(found == expected, "family parity");
  const auto progress = manifest["shell_reset_progress"].toObject();
  const auto acceptance = progress["acceptance"].toObject();
  QSet<QString> recorded;
  for (auto item : acceptance["blocked_families"].toArray()) {
    require(!recorded.contains(item.toString()), "duplicate blocker");
    recorded.insert(item.toString());
  }
  require(recorded == blocked, "blocked-family parity");
  require(acceptance["pending_workflows"].isArray(), "pending workflows inventory");
  for (auto workflow : acceptance["pending_workflows"].toArray())
    require(!workflow.toString().trimmed().isEmpty(), "empty workflow");
  const bool ready = blocked.isEmpty() && acceptance["pending_workflows"].toArray().isEmpty();
  require(acceptance["complete"].isBool() && acceptance["complete"].toBool() == ready,
          "acceptance readiness");
  require(progress["completed_through"].toInt() == (ready ? 136 : 135), "completion boundary");
  require(progress["current_block"].toInt() == (ready ? 137 : 136), "current boundary");
  return errors;
}
QJsonObject manifest() {
  return read_json("docs/gui-feature-coverage-manifest-mvp9.json");
}
QJsonObject baseline() {
  return read_json("docs/gui-feature-coverage-manifest-mvp7.json");
}
} // namespace

TEST_CASE("Replacement shell coverage resolves every current proof and records all acceptance gaps",
          "[gui][shell-coverage][integration][gui-feature-coverage]") {
  const auto current = manifest();
  const auto errors = audit(current, baseline());
  INFO(errors.join('\n').toStdString());
  CHECK(errors.isEmpty());
  // Numbered completion belongs to the plan, not to this inventory test.
  QFile plan(root + "docs/mvp-plan.md");
  REQUIRE(plan.open(QIODevice::ReadOnly));
  const auto text = QString::fromUtf8(plan.readAll());
  const auto progress = current["shell_reset_progress"].toObject();
  CHECK(text.contains(QStringLiteral("implemented_through: Block %1\n")
                          .arg(progress["completed_through"].toInt())));
  CHECK(
      text.contains(QStringLiteral("current_block: %1\n").arg(progress["current_block"].toInt())));
  if (qEnvironmentVariableIntValue("BLCAD_REQUIRE_SHELL_ACCEPTANCE") == 1)
    CHECK(progress["acceptance"].toObject()["complete"].toBool());
}
TEST_CASE(
    "Coverage inventory rejects lost families stale tags headless sources and false completion",
    "[gui][shell-coverage][integration][gui-feature-coverage]") {
  const auto accepted = baseline();
  const auto original = manifest();
  auto reject = [&](auto mutate) {
    auto candidate = original;
    mutate(candidate);
    CHECK_FALSE(audit(candidate, accepted).isEmpty());
  };
  reject([](auto& candidate) {
    auto families = candidate["families"].toArray();
    families.removeAt(0);
    candidate["families"] = families;
  });
  reject([](auto& candidate) {
    auto families = candidate["families"].toArray();
    families.append(families.first());
    candidate["families"] = families;
  });
  const auto change_proof = [&](const QString& filter, const QString& source) {
    reject([&](auto& candidate) {
      auto families = candidate["families"].toArray();
      auto family = families.first().toObject();
      auto shell = family["shell"].toObject();
      shell["proofs"] = QJsonArray{QJsonObject{{"filter", filter}, {"source", source}}};
      family["shell"] = shell;
      families[0] = family;
      candidate["families"] = families;
    });
  };
  change_proof("[gui][view-navigation-aids]", "tests/gui/gui_shell_window_tests.cpp");
  change_proof("[gui][document-session]", "tests/gui/gui_document_session_tests.cpp");
  change_proof("[gui][document-lifecycle],[deleted-proof]", "tests/gui/gui_shell_window_tests.cpp");
  reject([](auto& candidate) {
    auto progress = candidate["shell_reset_progress"].toObject();
    auto acceptance = progress["acceptance"].toObject();
    acceptance["complete"] = true;
    progress["acceptance"] = acceptance;
    candidate["shell_reset_progress"] = progress;
  });
}
TEST_CASE("Every recorded shell entry point belongs to the constructed window and ribbon",
          "[gui][shell-coverage][integration][gui-feature-coverage]") {
  ShellWindow window;
  const auto current = manifest();
  for (auto item : current["families"].toArray()) {
    const auto family = item.toObject();
    INFO(family["family"].toString().toStdString());
    const auto shell = family["shell"].toObject();
    for (auto action_id : shell["actions"].toArray()) {
      const auto name = "blcad.action." + action_id.toString();
      const auto actions = window.findChildren<QAction*>(name);
      INFO(name.toStdString());
      REQUIRE(actions.size() == 1);
      bool button_found = false;
      for (auto* button : window.ribbon().findChildren<QToolButton*>())
        button_found = button_found || button->defaultAction() == actions.first();
      CHECK(button_found);
    }
    for (auto widget_id : shell["widgets"].toArray()) {
      INFO(widget_id.toString().toStdString());
      CHECK(window.findChildren<QWidget*>(widget_id.toString()).size() == 1);
    }
  }
}
