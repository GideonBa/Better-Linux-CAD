#include "blcad/gui/shell/shell_feature_tools.hpp"

#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <charconv>
#include <cmath>

namespace blcad::gui {
namespace {
std::optional<double> scalar(const QString& text, std::string_view unit) {
  const auto input = text.trimmed().toStdString();
  double value = 0;
  const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
  if (end == input.data() || error != std::errc() || !std::isfinite(value))
    return std::nullopt;
  const auto suffix =
      QString::fromStdString(std::string(end, input.data() + input.size())).trimmed();
  if (!suffix.isEmpty() && suffix.toStdString() != unit)
    return std::nullopt;
  return value;
}
bool distance_extent(ExtrudeExtentMode mode) {
  return mode == ExtrudeExtentMode::Distance || mode == ExtrudeExtentMode::Symmetric ||
         mode == ExtrudeExtentMode::TwoSided;
}
} // namespace

void ShellFeatureTools::build_extrude_controls(QWidget* panel) {
  extrude_controls_ = new QWidget(panel);
  extrude_controls_->setObjectName(QStringLiteral("blcad.extrude.inputs"));
  auto* column = new QVBoxLayout(extrude_controls_);
  column->setContentsMargins(0, 0, 0, 0);
  extrude_form_ = new QFormLayout();
  extrude_form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
  column->addLayout(extrude_form_);
  const auto combo = [&](const char* id, const QString& label) {
    auto* field = new QComboBox(extrude_controls_);
    field->setObjectName(QString::fromLatin1(id));
    field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    field->setMinimumContentsLength(15);
    extrude_form_->addRow(label, field);
    return field;
  };
  extrude_extent_ = combo("blcad.extrude.extent", QStringLiteral("Begrenzung"));
  for (const auto& [label, mode] : {std::pair{"Abstand", ExtrudeExtentMode::Distance},
                                    {"Symmetrisch", ExtrudeExtentMode::Symmetric},
                                    {"Zwei Seiten", ExtrudeExtentMode::TwoSided},
                                    {"Durch alles", ExtrudeExtentMode::ThroughAll},
                                    {"Bis zum nächsten", ExtrudeExtentMode::ToNext},
                                    {"Bis Fläche", ExtrudeExtentMode::ToFace},
                                    {"Zwischen Flächen", ExtrudeExtentMode::Between}})
    extrude_extent_->addItem(QString::fromUtf8(label), static_cast<int>(mode));
  extrude_operation_ = combo("blcad.extrude.operation", QStringLiteral("Operation"));
  for (const auto& [label, mode] : {std::pair{"Neuer Körper", FeatureBodyOperationMode::NewBody},
                                    {"Vereinigen", FeatureBodyOperationMode::Join},
                                    {"Ausschneiden", FeatureBodyOperationMode::Cut},
                                    {"Schnittmenge", FeatureBodyOperationMode::Intersect}})
    extrude_operation_->addItem(QString::fromUtf8(label), static_cast<int>(mode));
  extrude_target_ = combo("blcad.extrude.target", QStringLiteral("Zielkörper"));
  extrude_direction_ = combo("blcad.extrude.direction", QStringLiteral("Richtung"));
  extrude_direction_->addItem(QStringLiteral("Skizzennormale"));
  extrude_direction_->addItem(QStringLiteral("Entgegengesetzt"));
  extrude_second_distance_ = new QLineEdit(QStringLiteral("5"), extrude_controls_);
  extrude_second_distance_->setObjectName(QStringLiteral("blcad.extrude.second_distance"));
  extrude_form_->addRow(QStringLiteral("Zweite Tiefe (mm)"), extrude_second_distance_);
  extrude_first_face_ = combo("blcad.extrude.first_face", QStringLiteral("Erste Fläche"));
  extrude_second_face_ = combo("blcad.extrude.second_face", QStringLiteral("Zweite Fläche"));
  extrude_taper_ = new QLineEdit(extrude_controls_);
  extrude_taper_->setObjectName(QStringLiteral("blcad.extrude.taper"));
  extrude_taper_->setPlaceholderText(QStringLiteral("Optional, -90 < Winkel < 90"));
  extrude_form_->addRow(QStringLiteral("Formschräge (deg)"), extrude_taper_);
  auto* hint = new QLabel(
      QStringLiteral("Durch alles und Bis zum nächsten benötigen einen Zielkörper. "
                     "Begrenzungsflächen müssen senkrecht zur "
                     "Extrusionsrichtung liegen. Alle Eingaben bleiben bis Anwenden lokal."),
      extrude_controls_);
  hint->setWordWrap(true);
  column->addWidget(hint);
  qobject_cast<QVBoxLayout*>(panel->layout())->addWidget(extrude_controls_);
  for (auto* field : {extrude_extent_, extrude_operation_, extrude_target_, extrude_direction_,
                      extrude_first_face_, extrude_second_face_})
    connect(field, &QComboBox::currentIndexChanged, this, [this] { refresh_extrude_preview(); });
  for (auto* field : {extrude_second_distance_, extrude_taper_}) {
    connect(field, &QLineEdit::textEdited, this, [this] { invalidate_extrude_preview(); });
    connect(field, &QLineEdit::editingFinished, this, [this] { refresh_extrude_preview(); });
  }
  extrude_controls_->hide();
}

void ShellFeatureTools::reset_extrude_controls(GuiFeatureManipulatorFrame frame) {
  extrude_frame_ = frame;
  const QSignalBlocker b1(extrude_extent_), b2(extrude_operation_), b3(extrude_target_),
      b4(extrude_direction_), b5(extrude_first_face_), b6(extrude_second_face_);
  extrude_extent_->setCurrentIndex(0);
  extrude_operation_->setCurrentIndex(0);
  extrude_direction_->setCurrentIndex(0);
  extrude_second_distance_->setText(QStringLiteral("5"));
  extrude_taper_->clear();
  extrude_target_->clear();
  extrude_first_face_->clear();
  extrude_second_face_->clear();
  extrude_faces_.clear();
  const auto& part = *session_.part_document();
  const auto* cache = session_.part_shape_cache();
  if (cache) {
    for (const auto& body : part.bodies())
      if (body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
          cache->find_body_shape(body.id()))
        extrude_target_->addItem(QString::fromStdString(body.name()),
                                 QString::fromStdString(body.id().value()));
    auto sources = geometry::ViewportSceneBuilder{}.build_construction_sources(part, *cache);
    if (sources)
      for (const auto& source : sources.value())
        if (const auto* face = std::get_if<SemanticFaceReference>(&source.reference)) {
          auto reference =
              FaceReference::create_planar(PartFeatureInputRole::ExtrudeLimitFace, *face);
          if (!reference)
            continue;
          const auto* feature = part.find_feature(face->source_feature());
          const auto face_label = [&] {
            switch (face->face()) {
            case SemanticFace::Top:
              return QStringLiteral("Oben");
            case SemanticFace::Bottom:
              return QStringLiteral("Unten");
            case SemanticFace::Left:
              return QStringLiteral("Links");
            case SemanticFace::Right:
              return QStringLiteral("Rechts");
            case SemanticFace::Front:
              return QStringLiteral("Vorne");
            case SemanticFace::Back:
              return QStringLiteral("Hinten");
            }
            return QString::fromUtf8(to_string(face->face()).data());
          }();
          const auto label =
              QString::fromStdString(feature->name()) + QStringLiteral(" / ") + face_label;
          const auto identity = QString::fromStdString(face->source_feature().value() + "/" +
                                                       std::string(to_string(face->face())));
          extrude_faces_.push_back(reference.value());
          extrude_first_face_->addItem(label, identity);
          extrude_second_face_->addItem(label, identity);
        }
  }
  if (extrude_second_face_->count() > 1)
    extrude_second_face_->setCurrentIndex(1);
  update_extrude_controls();
}

void ShellFeatureTools::update_extrude_controls() {
  const auto mode = static_cast<ExtrudeExtentMode>(extrude_extent_->currentData().toInt());
  const bool distance = distance_extent(mode);
  numeric_editor_->setVisible(distance);
  value_label_->setVisible(distance);
  extrude_form_->setRowVisible(extrude_second_distance_, mode == ExtrudeExtentMode::TwoSided);
  extrude_form_->setRowVisible(extrude_first_face_, mode == ExtrudeExtentMode::ToFace ||
                                                        mode == ExtrudeExtentMode::Between);
  extrude_form_->setRowVisible(extrude_second_face_, mode == ExtrudeExtentMode::Between);
  extrude_form_->setRowVisible(extrude_target_, extrude_operation_->currentIndex() != 0);
}

void ShellFeatureTools::invalidate_extrude_preview() {
  if (command_ != Command::Extrude)
    return;
  preview_valid_ = false;
  viewport_.clear_feature_preview();
  refresh_enablement();
  synchronize_overlay();
}

void ShellFeatureTools::refresh_extrude_preview() {
  if (command_ != Command::Extrude)
    return;
  update_extrude_controls();
  auto preview = preview_and_publish();
  if (!preview)
    report(preview.error());
}

Result<std::size_t> ShellFeatureTools::configure_extrude_candidate() {
  const auto fail = [](std::string message) {
    return Result<std::size_t>::failure(Error::validation("extrude", std::move(message)));
  };
  if (!layer_.active_handle_id().empty())
    return Result<std::size_t>::success(0);
  auto& controller = coordinator_.extrude();
  const auto mode = static_cast<ExtrudeExtentMode>(extrude_extent_->currentData().toInt());
  const auto operation =
      static_cast<FeatureBodyOperationMode>(extrude_operation_->currentData().toInt());
  if (distance_extent(mode)) {
    const auto depth = scalar(numeric_editor_->text(), "mm");
    if (!depth || *depth <= 0)
      return fail("Extrusionstiefe muss eine positive Länge sein.");
  }
  if (operation == FeatureBodyOperationMode::NewBody &&
      (mode == ExtrudeExtentMode::ThroughAll || mode == ExtrudeExtentMode::ToNext))
    return fail("Diese Begrenzung benötigt eine Operation mit Zielkörper.");
  if (operation != FeatureBodyOperationMode::NewBody && extrude_target_->currentIndex() < 0)
    return fail("Einen sichtbaren Volumenkörper als Ziel auswählen.");
  controller.set_operation(
      operation,
      operation == FeatureBodyOperationMode::NewBody
          ? std::nullopt
          : std::optional<BodyId>{BodyId(extrude_target_->currentData().toString().toStdString())});
  controller.set_extent_mode(mode);
  if (mode == ExtrudeExtentMode::TwoSided) {
    const auto depth = scalar(extrude_second_distance_->text(), "mm");
    if (!depth || *depth <= 0)
      return fail("Zweite Tiefe muss eine positive Länge sein.");
    const auto id = controller.distance_parameter_id().value() + ".second";
    auto parameter =
        Parameter::create_length(ParameterId(id), controller.feature_id().value() + " zweite Tiefe",
                                 Quantity::length_mm(*depth, id).value());
    if (!parameter)
      return Result<std::size_t>::failure(parameter.error());
    controller.set_two_sided_new_parameter(std::move(parameter.value()));
  }
  std::optional<double> taper;
  if (!extrude_taper_->text().trimmed().isEmpty()) {
    taper = scalar(extrude_taper_->text(), "deg");
    if (!taper || *taper <= -90 || *taper >= 90)
      return fail("Formschräge muss zwischen -90 und 90 Grad liegen.");
  }
  controller.set_taper(taper);
  std::optional<FaceReference> first, second;
  if (mode == ExtrudeExtentMode::ToFace || mode == ExtrudeExtentMode::Between) {
    if (extrude_first_face_->currentIndex() < 0)
      return fail("Eine erste ebene Begrenzungsfläche auswählen.");
    first = extrude_faces_.at(static_cast<std::size_t>(extrude_first_face_->currentIndex()));
  }
  if (mode == ExtrudeExtentMode::Between) {
    if (extrude_second_face_->currentIndex() < 0)
      return fail("Eine zweite ebene Begrenzungsfläche auswählen.");
    second = extrude_faces_.at(static_cast<std::size_t>(extrude_second_face_->currentIndex()));
  }
  controller.set_limit_faces(std::move(first), std::move(second));
  const bool opposite = extrude_direction_->currentIndex() == 1;
  controller.set_direction(opposite ? ExtrudeDirection::OppositeSketchNormal
                                    : ExtrudeDirection::SketchNormal);
  auto frame = extrude_frame_;
  if (opposite)
    frame.axis = {-frame.axis.x, -frame.axis.y, -frame.axis.z};
  controller.set_manipulator_frame(frame);
  coordinator_.refresh_handles();
  return Result<std::size_t>::success(1);
}
} // namespace blcad::gui
