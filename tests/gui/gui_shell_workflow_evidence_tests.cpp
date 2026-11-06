#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QSysInfo>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_registry_hub.hpp>
#include <catch2/interfaces/catch_interfaces_testcase.hpp>
#include <catch2/internal/catch_test_spec_parser.hpp>

#include <algorithm>
#include <cmath>
#include <sys/resource.h>
#include <type_traits>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {
const QString root = QStringLiteral(BLCAD_SOURCE_DIR) + '/';

template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}

QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

QJsonObject program() {
  const auto document =
      QJsonDocument::fromJson(bytes(root + "docs/gui-feature-coverage-manifest-mvp9.json"));
  REQUIRE(document.isObject());
  return document.object()["workflow_program"].toObject();
}

bool repository_file(const QString& path) {
  const QFileInfo file(root + path);
  return !path.isEmpty() && !QFileInfo(path).isAbsolute() && file.isFile() &&
         file.canonicalFilePath().startsWith(root);
}

// Evidence registration is deliberately separate from execution. In particular,
// a registered xcb proof does not claim it ran on xcb during this audit.
QStringList audit(const QJsonObject& candidate) {
  QStringList errors;
  const auto require = [&](bool condition, const QString& message) {
    if (!condition)
      errors << message;
  };
  require(candidate["schema"] == "blcad.workflow-capability-evidence.v1", "schema");
  require(repository_file(candidate["contract"].toString()), "program contract");
  require(repository_file(candidate["test_contract"].toString()), "test contract");
  require(candidate["capabilities"].isArray(), "capability array");
  const auto completed = QRegularExpression("^implemented_through: Block ([0-9]+)$",
                                            QRegularExpression::MultilineOption)
                             .match(QString::fromUtf8(bytes(root + "docs/mvp-plan.md")));
  require(completed.hasMatch(), "numbered implementation authority");
  QSet<QString> found;
  const auto& registry = Catch::getRegistryHub();
  for (const auto value : candidate["capabilities"].toArray()) {
    const auto entry = value.toObject();
    const auto id = entry["capability"].toString();
    require(QRegularExpression("^[a-z][a-z0-9-]+$").match(id).hasMatch() && !found.contains(id),
            "capability identity: " + id);
    found.insert(id);
    require(repository_file(entry["contract"].toString()), "capability contract: " + id);
    const auto owner = entry["owner"].toInt();
    require(entry["owner"].isDouble() && owner == entry["owner"].toDouble() && owner >= 1 &&
                owner <= 204,
            "capability owner: " + id);
    for (const auto key : {"commands", "apis", "proofs"})
      require(entry[key].isArray(), QString::fromLatin1(key) + ": " + id);
    const auto disposition = entry["disposition"].toString();
    if (disposition == "planned") {
      require(owner >= 145 && entry["commands"].toArray().isEmpty() &&
                  entry["apis"].toArray().isEmpty() && entry["proofs"].toArray().isEmpty(),
              "planned capability claims implementation: " + id);
      require(entry["planned_commands"].isArray() && !entry["planned_commands"].toArray().isEmpty(),
              "planned command names: " + id);
      QSet<QString> labels;
      for (const auto label : entry["planned_commands"].toArray()) {
        const auto text = label.toString().trimmed();
        require(!text.isEmpty() && !labels.contains(text), "planned command label: " + id);
        labels.insert(text);
      }
      continue;
    }
    require(disposition == "implemented", "disposition: " + id);
    require(completed.hasMatch() && owner <= completed.captured(1).toInt(),
            "capability exceeds implemented boundary: " + id);
    require(!entry.contains("planned_commands"), "mixed planned/implemented entry: " + id);
    require(!entry["commands"].toArray().isEmpty() && !entry["apis"].toArray().isEmpty(),
            "implemented command/API mapping: " + id);
    QSet<QString> commands;
    for (const auto command : entry["commands"].toArray()) {
      const auto name = command.toString();
      require(name.startsWith("blcad.action.") && !commands.contains(name),
              "action identity: " + id);
      commands.insert(name);
    }
    for (const auto api_value : entry["apis"].toArray()) {
      const auto api = api_value.toObject();
      const auto source = api["source"].toString();
      const auto type = api["type"].toString();
      const bool valid_path = source.startsWith("include/blcad/gui/") && repository_file(source);
      require(valid_path, "API source: " + id);
      require(QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(type).hasMatch(),
              "API type: " + id);
      require(api["methods"].isArray() && !api["methods"].toArray().isEmpty(),
              "API methods: " + id);
      if (!valid_path)
        continue;
      const auto text = QString::fromUtf8(bytes(root + source));
      require(QRegularExpression("\\bclass\\s+" + QRegularExpression::escape(type) + "\\b")
                  .match(text)
                  .hasMatch(),
              "API declaration: " + id);
      for (const auto method : api["methods"].toArray()) {
        const auto name = method.toString();
        require(QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(name).hasMatch() &&
                    QRegularExpression("\\b" + QRegularExpression::escape(name) + "\\s*\\(")
                        .match(text)
                        .hasMatch(),
                "API method declaration: " + id);
      }
    }
    QSet<QString> kinds;
    for (const auto proof_value : entry["proofs"].toArray()) {
      const auto proof = proof_value.toObject();
      const auto kind = proof["kind"].toString();
      const auto filter = proof["filter"].toString();
      const auto source = proof["source"].toString();
      kinds.insert(kind);
      require((kind == "native" && proof["platform"] == "xcb") ||
                  (kind == "headless" && proof["platform"] == "any"),
              "proof execution kind: " + id);
      require(QRegularExpression(R"(^(\[[A-Za-z0-9_-]+\])+$)").match(filter).hasMatch(),
              "single positive proof filter: " + id);
      require(source.startsWith("tests/gui/gui_shell_") && repository_file(source),
              "proof source: " + id);
      const auto spec = Catch::TestSpecParser(registry.getTagAliasRegistry())
                            .parse(filter.toStdString())
                            .testSpec();
      require(spec.hasFilters() && spec.getInvalidSpecs().empty(), "proof filter: " + id);
      std::size_t matched = 0;
      for (const auto* test : registry.getTestCaseRegistry().getAllInfos()) {
        if (!spec.matches(*test))
          continue;
        ++matched;
        require(!test->isHidden(), "hidden proof: " + id);
        require(QFileInfo(QString::fromUtf8(test->lineInfo.file)).canonicalFilePath() ==
                    QFileInfo(root + source).canonicalFilePath(),
                "proof source mismatch: " + id);
      }
      require(matched > 0, "unresolved proof: " + id);
    }
    require(kinds == QSet<QString>{"headless", "native"}, "proof kind coverage: " + id);
  }
  const QSet<QString> expected{"existing-extrude",
                               "shared-property-workflow",
                               "feature-history",
                               "materials",
                               "semantic-holes",
                               "threads",
                               "planar-g2",
                               "equation-curves",
                               "spatial-constraints",
                               "curve-preserving-paths",
                               "g2-loft",
                               "offset-thicken",
                               "curved-surface-edit",
                               "advanced-fillet",
                               "rib-web",
                               "emboss-engrave",
                               "direct-edit",
                               "model-states",
                               "flexible-occurrences",
                               "adaptive-design",
                               "assembly-holes",
                               "motion-analysis",
                               "finite-measure",
                               "surface-analysis"};
  require(found == expected, "program capability coverage");
  return errors;
}

GuiDocumentSession fixture() {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.workflow-baseline"), "Workflow baseline"));
  (void)checked(session.commit_part_transaction("Seed baseline", [](PartDocument& part) {
    REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
    REQUIRE(part.add_parameter(checked(Parameter::create_length(
        ParameterId("width"), "width", checked(Quantity::length_mm(20))))));
    REQUIRE(part.add_parameter(checked(Parameter::create_length(
        ParameterId("height"), "height", checked(Quantity::length_mm(10))))));
    auto sketch =
        checked(Sketch::create(SketchId("sketch.baseline"), "Baseline", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
        ProfileId("profile.baseline"), ParameterId("width"), ParameterId("height")))));
    return part.add_sketch(std::move(sketch));
  }));
  return session;
}

GuiInteractiveExtrudeController controller(const GuiDocumentSession& session) {
  GuiInteractiveExtrudeController result;
  REQUIRE(result.begin_with_new_inputs(
      session, SketchId("sketch.baseline"), ProfileId("profile.baseline"),
      FeatureId("feature.baseline"), "Baseline extrusion",
      checked(
          Parameter::create_length(ParameterId("depth"), "depth", checked(Quantity::length_mm(8)))),
      checked(Body::create(BodyId("body.baseline"), "Baseline", BodyKind::Solid))));
  return result;
}

QJsonObject sample(GuiInteractiveExtrudeController& control, const GuiDocumentSession& session) {
  const auto before = checked(serialize_part_document_to_json(*session.part_document()));
  const auto revision = session.model_revision();
  const auto undo = std::string(session.undo_label());
  QElapsedTimer clock;
  clock.start();
  const auto shape = checked(control.preview_shape(session));
  const double elapsed = clock.nsecsElapsed() / 1e6;
  const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(shape);
  REQUIRE_FALSE(summary.is_null);
  REQUIRE(summary.solid_count == 1);
  REQUIRE(summary.volume_mm3 == Catch::Approx(1600).margin(1e-7));
  REQUIRE(checked(serialize_part_document_to_json(*session.part_document())) == before);
  REQUIRE(session.model_revision() == revision);
  REQUIRE(session.undo_label() == undo);
  return QJsonObject{{"preview_ms", elapsed},
                     {"volume_mm3", summary.volume_mm3},
                     {"solid_count", static_cast<int>(summary.solid_count)}};
}

QJsonArray runtime_inventory() {
  QStringList files;
  for (const auto dir : {"src", "include", "tests", "assets", "cmake", "scripts"}) {
    QDirIterator it(root + dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
      files << it.next().mid(root.size());
  }
  files << "CMakeLists.txt" << "CMakePresets.json";
  files.sort();
  QJsonArray entries;
  for (const auto& path : files)
    entries.append(QJsonObject{
        {"path", path},
        {"sha256",
         QString::fromLatin1(
             QCryptographicHash::hash(bytes(root + path), QCryptographicHash::Sha256).toHex())}});
  return entries;
}

QJsonObject timing_summary(const QJsonArray& samples) {
  std::vector<double> times;
  for (const auto value : samples)
    times.push_back(value.toObject()["preview_ms"].toDouble());
  REQUIRE_FALSE(times.empty());
  std::sort(times.begin(), times.end());
  const auto percentile = [&](double fraction) {
    return times[static_cast<std::size_t>(std::ceil(times.size() * fraction)) - 1];
  };
  return {{"p50_ms", percentile(.5)},
          {"p95_ms", percentile(.95)},
          {"observed_max_ms", times.back()},
          {"sample_count", static_cast<int>(times.size())}};
}
} // namespace

TEST_CASE("Advanced workflow capability mapping separates planned work from executable evidence",
          "[gui][shell-coverage][integration][gui-feature-coverage][parity-b145]") {
  const auto current = program();
  const auto errors = audit(current);
  INFO(errors.join('\n').toStdString());
  REQUIRE(errors.isEmpty());
  ShellWindow window;
  for (const auto value : current["capabilities"].toArray())
    for (const auto action : value.toObject()["commands"].toArray())
      CHECK(window.findChildren<QAction*>(action.toString()).size() == 1);
  // Declaration auditing is supplemented by compilation and the real baseline
  // calls below. Textual symbol existence alone does not establish correctness.
  STATIC_REQUIRE(std::is_member_function_pointer_v<
                 decltype(&GuiInteractiveExtrudeController::begin_with_new_inputs)>);
  STATIC_REQUIRE(
      std::is_member_function_pointer_v<decltype(&GuiInteractiveExtrudeController::preview_shape)>);
  STATIC_REQUIRE(
      std::is_member_function_pointer_v<decltype(&GuiInteractiveExtrudeController::apply)>);
}

TEST_CASE("Advanced workflow evidence rejects fabricated implementations and broken proof mappings",
          "[gui][shell-coverage][integration][gui-feature-coverage][parity-b145]") {
  const auto original = program();
  const auto reject = [&](auto mutate, int index = 0) {
    auto candidate = original;
    auto entries = candidate["capabilities"].toArray();
    auto entry = entries[index].toObject();
    mutate(entry);
    entries[index] = entry;
    candidate["capabilities"] = entries;
    CHECK_FALSE(audit(candidate).isEmpty());
  };
  reject([](auto& entry) { entry["disposition"] = "implemented"; }, 1);
  reject([](auto& entry) { entry["commands"] = QJsonArray{"blcad.action.fake"}; }, 1);
  reject([](auto& entry) { entry["proofs"] = QJsonArray{QJsonObject{}}; }, 1);
  reject([](auto& entry) { entry["planned_commands"] = QJsonArray{""}; }, 1);
  reject([](auto& entry) { entry["owner"] = 149.5; });
  reject([](auto& entry) { entry["owner"] = 205; });
  reject([](auto& entry) {
    const auto completed = QRegularExpression("^implemented_through: Block ([0-9]+)$",
                                              QRegularExpression::MultilineOption)
                               .match(QString::fromUtf8(bytes(root + "docs/mvp-plan.md")));
    REQUIRE(completed.hasMatch());
    entry["owner"] = completed.captured(1).toInt() + 1;
  });
  reject([](auto& entry) { entry["disposition"] = "maybe"; });
  reject([](auto& entry) { entry["contract"] = "../AGENTS.md"; });
  reject([](auto& entry) { entry["capability"] = "feature-history"; });
  reject([](auto& entry) {
    auto apis = entry["apis"].toArray();
    auto api = apis[0].toObject();
    api["methods"] = QJsonArray{"nonexistent_api"};
    apis[0] = api;
    entry["apis"] = apis;
  });
  for (const auto filter : {"[workflow-baseline],[deleted-proof]", "[deleted-proof]", ""})
    reject([&](auto& entry) {
      auto proofs = entry["proofs"].toArray();
      auto proof = proofs[0].toObject();
      proof["filter"] = filter;
      proofs[0] = proof;
      entry["proofs"] = proofs;
    });
  reject([](auto& entry) {
    auto proofs = entry["proofs"].toArray();
    auto proof = proofs[0].toObject();
    proof["source"] = "tests/gui/gui_shell_coverage_tests.cpp";
    proofs[0] = proof;
    entry["proofs"] = proofs;
  });
  reject([](auto& entry) {
    auto proofs = entry["proofs"].toArray();
    auto proof = proofs[1].toObject();
    proof["platform"] = "offscreen";
    proofs[1] = proof;
    entry["proofs"] = proofs;
  });
  auto missing = original;
  auto entries = missing["capabilities"].toArray();
  entries.removeLast();
  missing["capabilities"] = entries;
  CHECK_FALSE(audit(missing).isEmpty());
}

TEST_CASE("Existing Extrude baseline records checked geometry without changing committed intent",
          "[gui][parity-b145][workflow-baseline]") {
  QJsonArray fresh, reused;
  for (int i = 0; i < 3; ++i) {
    auto session = fixture();
    auto control = controller(session);
    fresh.append(sample(control, session));
  }
  auto session = fixture();
  auto control = controller(session);
  for (int i = 0; i < 10; ++i)
    reused.append(sample(control, session));
  const auto source = checked(serialize_part_document_to_json(*session.part_document()));
  auto roundtrip = checked(deserialize_part_document_from_json(source));
  CHECK(checked(serialize_part_document_to_json(roundtrip)) == source);
  REQUIRE(control.apply(session));
  REQUIRE(session.part_document()->features().size() == 1);
  REQUIRE(session.undo());
  CHECK(checked(serialize_part_document_to_json(*session.part_document())) == source);
  REQUIRE(session.redo());
  const auto* shape = session.part_shape_cache()->find_body_shape(BodyId("body.baseline"));
  REQUIRE(shape);
  CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
        Catch::Approx(1600).margin(1e-7));
  const auto output = qEnvironmentVariable("BLCAD_WORKFLOW_BASELINE");
  if (!output.isEmpty()) {
    const auto inventory = runtime_inventory();
    QString cpu;
    QFile cpuinfo("/proc/cpuinfo");
    if (cpuinfo.open(QIODevice::ReadOnly))
      for (const auto& line : cpuinfo.readAll().split('\n'))
        if (line.startsWith("model name")) {
          cpu = QString::fromUtf8(line.mid(line.indexOf(':') + 1)).trimmed();
          break;
        }
    rusage usage{};
    REQUIRE(getrusage(RUSAGE_SELF, &usage) == 0);
    const QJsonObject report{
        {"schema", "blcad.workflow-baseline.v1"},
        {"captured_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"measurement", "preview_shape only; fresh session is not cold process; no latency gate"},
        {"qt", qVersion()},
        {"occt", BLCAD_WORKFLOW_OCCT_VERSION},
        {"compiler", __VERSION__},
        {"build_type", BLCAD_WORKFLOW_BUILD_TYPE},
        {"cpu", cpu},
        {"process_peak_rss_kib", static_cast<double>(usage.ru_maxrss)},
        {"kernel", QSysInfo::kernelVersion()},
        {"os", QSysInfo::prettyProductName()},
        {"architecture", QSysInfo::currentCpuArchitecture()},
        {"platform", QApplication::platformName()},
        {"device_pixel_ratio", QGuiApplication::primaryScreen()->devicePixelRatio()},
        {"source_json", QString::fromStdString(source)},
        {"source_json_sha256",
         QString::fromLatin1(
             QCryptographicHash::hash(QByteArray::fromStdString(source), QCryptographicHash::Sha256)
                 .toHex())},
        {"fresh_session_samples", fresh},
        {"reused_controller_samples", reused},
        {"fresh_session_summary", timing_summary(fresh)},
        {"reused_controller_summary", timing_summary(reused)},
        {"runtime_inventory", inventory},
        {"runtime_inventory_sha256",
         QString::fromLatin1(
             QCryptographicHash::hash(QJsonDocument(inventory).toJson(QJsonDocument::Compact),
                                      QCryptographicHash::Sha256)
                 .toHex())}};
    QSaveFile file(output);
    REQUIRE(file.open(QIODevice::WriteOnly));
    const auto data = QJsonDocument(report).toJson(QJsonDocument::Indented);
    REQUIRE(file.write(data) == data.size());
    REQUIRE(file.commit());
  }
}
