#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "sketch_driven_tools_impl.hpp"
#include <QHeaderView>
#include <QSignalBlocker>
#include <charconv>
#include <cmath>
namespace blcad::gui {
namespace {
QString q(const std::string& s) {
  return QString::fromStdString(s);
}
Error error(const std::string& s) {
  return Error::validation("part.sketch_driven", s);
}
template <class T> T take(Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().message());
  return std::move(r.value());
}
double number(const QLineEdit* field) {
  const auto s = field->text().trimmed().toStdString();
  double out;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
  if (r.ec != std::errc{} || r.ptr != s.data() + s.size() || !std::isfinite(out))
    throw std::runtime_error("Enter a finite number using a decimal point.");
  return out;
}
FaceReference role(const FaceReference& f, PartFeatureInputRole wanted) {
  return std::visit(
      [&](const auto& s) -> FaceReference {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, ResultFaceReference>)
          return take(FaceReference::create_result(wanted, s));
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return take(FaceReference::create_imported(wanted, s));
        else if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return take(FaceReference::create_planar(wanted, s));
        else
          return take(FaceReference::create_cylindrical(wanted, s));
      },
      f.source());
}
} // namespace
void ShellSketchDrivenTools::Impl::reload_sources(bool clear) {
  const QSignalBlocker a(ui->curve), b(ui->profile);
  const auto previous = ui->profile->currentData();
  ui->curve->clear();
  ui->profile->clear();
  if (clear)
    ui->curves->clear();
  const auto* sketch = window.session().part_document()->find_sketch(
      SketchId(ui->sketch->currentData().toString().toStdString()));
  if (!sketch)
    return;
  const auto curves = [&](const auto& records, const char* type) {
    for (const auto& r : records)
      if (!sketch->curve_is_construction(r.id()))
        ui->curve->addItem(QString(type) + " / " + q(r.id().value()), q(r.id().value()));
  };
  curves(sketch->line_segments(), "Line");
  curves(sketch->arc_segments(), "Arc");
  curves(sketch->spline_segments(), "Spline");
  curves(sketch->equation_curves(), "Equation curve");
  const auto profiles = [&](const auto& records, const char* type) {
    for (const auto& r : records)
      if (!sketch->profile_has_construction(r.id()))
        ui->profile->addItem(QString(type) + " / " + q(r.id().value()), q(r.id().value()));
  };
  profiles(sketch->rectangle_profiles(), "Rectangle");
  profiles(sketch->circle_profiles(), "Circle");
  profiles(sketch->closed_profiles(), "Closed profile");
  profiles(sketch->arc_closed_profiles(), "Curved profile");
  profiles(sketch->composite_closed_profiles(), "Composite region");
  int selected = ui->profile->findData(previous);
  if (selected >= 0)
    ui->profile->setCurrentIndex(selected);
}
void ShellSketchDrivenTools::Impl::reload_targets() {
  if (!inputs || !input_cache)
    return;
  const QSignalBlocker a(ui->face), b(ui->limit_face);
  const auto old_face = ui->face->currentData(), old_limit = ui->limit_face->currentData();
  targets.clear();
  ui->face->clear();
  ui->limit_face->clear();
  for (const auto& body : inputs->bodies()) {
    if (body.kind() != BodyKind::Solid || !input_cache->find_body_shape(body.id()))
      continue;
    auto queried = query_with_step_import_context(window.session(), [&](auto* context) {
      return geometry::SketchDrivenGuideQuery::faces(*inputs, *input_cache, body.id(), context,
                                                     prefix);
    });
    if (!queried)
      continue;
    for (auto& target : queried.value()) {
      const auto& ref = std::get<FaceReference>(target.selection.reference);
      const auto key = interactive_face_identity(ref);
      face_references.insert_or_assign(key, ref);
      const auto label = q(body.name() + " / " + target.label);
      if (body.id().value() == ui->body->currentData().toString().toStdString())
        ui->face->addItem(label, q(key));
      if (target.mapping == EmbossMapping::Planar)
        ui->limit_face->addItem(label, q(key));
      targets.push_back(std::move(target));
    }
  }
  int selected = ui->face->findData(old_face);
  if (selected >= 0)
    ui->face->setCurrentIndex(selected);
  selected = ui->limit_face->findData(old_limit);
  if (selected >= 0)
    ui->limit_face->setCurrentIndex(selected);
}
void ShellSketchDrivenTools::Impl::add_curve() {
  if (!current() || ui->curve->currentIndex() < 0)
    return;
  auto id = ui->curve->currentData();
  for (int i = 0; i < ui->curves->count(); ++i)
    if (ui->curves->item(i)->data(Qt::UserRole) == id) {
      fail(error("This curve is already selected."));
      return;
    }
  auto* item = new QListWidgetItem(ui->curve->currentText(), ui->curves);
  item->setData(Qt::UserRole, id);
  edited();
}
void ShellSketchDrivenTools::Impl::add_face() {
  if (!current() || ui->face->currentIndex() < 0)
    return;
  auto id = ui->face->currentData();
  for (int i = 0; i < ui->supports->count(); ++i)
    if (ui->supports->item(i)->data(Qt::UserRole) == id) {
      fail(error("This support face is already selected."));
      return;
    }
  auto* item = new QListWidgetItem(ui->face->currentText(), ui->supports);
  item->setData(Qt::UserRole, id);
  edited();
}
void ShellSketchDrivenTools::Impl::add_token(const SketchTextBinding* source) {
  int row = ui->tokens->rowCount();
  ui->tokens->insertRow(row);
  ui->tokens->setItem(
      row, 0, new QTableWidgetItem(source ? q(source->token) : QString("n%1").arg(row + 1)));
  auto* parameter = new QComboBox(ui->tokens);
  for (const auto& p : window.session().part_document()->parameters())
    parameter->addItem(q(p.name()), q(p.id().value()));
  if (source)
    parameter->setCurrentIndex(parameter->findData(q(source->parameter.value())));
  ui->tokens->setCellWidget(row, 1, parameter);
  auto* digits = new QSpinBox(ui->tokens);
  digits->setRange(0, 12);
  digits->setValue(source ? int(source->precision) : 0);
  ui->tokens->setCellWidget(row, 2, digits);
  QObject::connect(parameter, &QComboBox::currentIndexChanged, &owner, [this] { edited(); });
  QObject::connect(digits, &QSpinBox::valueChanged, &owner, [this] { edited(); });
  ui->tokens->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  edited();
}
void ShellSketchDrivenTools::Impl::bind(detail::SketchDrivenControls::Binding& b,
                                        std::optional<ParameterId> id) {
  ui->bind_catalog(*window.session().part_document(), b, id.value_or(ParameterId("")));
}
Result<ParameterId> ShellSketchDrivenTools::Impl::parameter(
    PartDocument& p, const detail::SketchDrivenControls::Binding& b, const char* suffix) const {
  try {
    const auto selected = b.parameter->currentData().toString().toStdString();
    const double v = number(b.value);
    auto quantity =
        b.type == ParameterType::Angle ? Quantity::angle_deg(v) : Quantity::length_mm(v);
    if (!quantity)
      return Result<ParameterId>::failure(quantity.error());
    if (!selected.empty()) {
      ParameterId id(selected);
      const auto* old = p.find_parameter(id);
      if (!old || old->type() != b.type)
        return Result<ParameterId>::failure(
            error("The bound parameter is unavailable or has the wrong type."));
      const double original =
          b.type == ParameterType::Angle ? old->value().degrees() : old->value().millimeters();
      if (v != original) {
        if (old->is_expression())
          return Result<ParameterId>::failure(
              error("Formula values are read-only. Choose New value to rebind explicitly."));
        auto updated = p.set_parameter_value(id, quantity.value());
        if (!updated)
          return Result<ParameterId>::failure(updated.error());
      }
      return Result<ParameterId>::success(id);
    }
    ParameterId id(feature.value() + "." + suffix);
    std::string name = ui->name->text().toStdString() + " " + suffix;
    const auto original_name = name;
    for (std::size_t i = 1; p.find_parameter(id) || p.find_parameter(std::string_view(name)); ++i) {
      id = ParameterId(feature.value() + "." + suffix + ".independent." + std::to_string(i));
      name = original_name + " " + std::to_string(i);
    }
    auto parameter = b.type == ParameterType::Angle
                         ? Parameter::create_angle(id, name, quantity.value())
                         : Parameter::create_length(id, name, quantity.value());
    if (!parameter)
      return Result<ParameterId>::failure(parameter.error());
    auto added = p.add_parameter(parameter.value());
    if (!added)
      return Result<ParameterId>::failure(added.error());
    return Result<ParameterId>::success(id);
  } catch (const std::exception& e) {
    return Result<ParameterId>::failure(error(e.what()));
  }
}
Result<PartDocument> ShellSketchDrivenTools::Impl::candidate() const {
  try {
    if (!current())
      throw std::runtime_error("The model changed. Reopen this feature editor.");
    auto p = *window.session().part_document();
    const BodyId body(ui->body->currentData().toString().toStdString());
    const SketchId sketch(ui->sketch->currentData().toString().toStdString());
    auto size = take(parameter(p, ui->size, "size"));
    std::optional<SketchDrivenFeature> record;
    if (ui->mode->currentIndex() < 2) {
      RibSketchSource source{sketch, {}, std::nullopt};
      if (ui->source_kind->currentIndex() == 0)
        for (int i = 0; i < ui->curves->count(); ++i)
          source.curves.push_back(
              SketchEntityId(ui->curves->item(i)->data(Qt::UserRole).toString().toStdString()));
      else if (ui->source_kind->currentIndex() == 1)
        source.profile = ProfileId(ui->profile->currentData().toString().toStdString());
      else
        throw std::runtime_error("Rib/Web requires open curves or a closed profile.");
      RibExtent extent = RibDistanceExtent{ParameterId("")};
      if (ui->extent->currentIndex() == 0)
        extent = RibDistanceExtent{take(parameter(p, ui->distance, "distance"))};
      else if (ui->extent->currentIndex() == 1) {
        auto at = face_references.find(ui->limit_face->currentData().toString().toStdString());
        if (at == face_references.end())
          throw std::runtime_error("Select an available termination face.");
        extent = RibFaceExtent{role(at->second, PartFeatureInputRole::ExtrudeLimitFace)};
      } else
        extent = RibBodyExtent{BodyId(ui->limit_body->currentData().toString().toStdString())};
      RibWebIntent intent{
          ui->mode->currentIndex() == 0 ? RibWebKind::Rib : RibWebKind::Web,
          source,
          ui->plane->currentIndex() == 0 ? RibExtrusionPlane::PerpendicularToSketch
                                         : RibExtrusionPlane::ParallelToSketch,
          {number(ui->direction[0]), number(ui->direction[1]), number(ui->direction[2])},
          static_cast<RibThicknessSide>(ui->side->currentIndex()),
          size,
          std::nullopt,
          std::nullopt,
          extent,
          ui->crossings->currentIndex() == 0 ? RibCrossingPolicy::Union
                                             : RibCrossingPolicy::Reject};
      if (ui->side->currentIndex() == 3)
        intent.second_thickness = take(parameter(p, ui->second, "second"));
      if (ui->draft_enabled->isChecked())
        intent.draft = take(parameter(p, ui->draft, "draft"));
      record = take(SketchDrivenFeature::create_rib_web(
          feature, ui->name->text().trimmed().toStdString(), body, intent));
    } else {
      std::optional<EmbossSource> source;
      if (ui->source_kind->currentIndex() == 2) {
        std::vector<SketchTextBinding> bindings;
        for (int row = 0; row < ui->tokens->rowCount(); ++row) {
          const auto* token = ui->tokens->item(row, 0);
          auto* parameter = qobject_cast<QComboBox*>(ui->tokens->cellWidget(row, 1));
          auto* digits = qobject_cast<QSpinBox*>(ui->tokens->cellWidget(row, 2));
          if (!token || !parameter || !digits)
            throw std::runtime_error("Complete each text binding.");
          bindings.push_back({token->text().trimmed().toStdString(),
                              ParameterId(parameter->currentData().toString().toStdString()),
                              std::size_t(digits->value())});
        }
        auto font = bundled_outline_font(ui->font->currentText().toStdString());
        if (!font)
          throw std::runtime_error("Choose a bundled outline font.");
        auto text = take(SketchText::create(
            original_text ? original_text->text.id() : SketchTextId(feature.value() + ".text"),
            sketch, ui->text->text().toStdString(), ui->font->currentText().toStdString(),
            take(parameter(p, ui->height, "height")),
            original_text ? original_text->text.anchor() : Point2{},
            original_text ? original_text->text.rotation_parameter() : std::nullopt, bindings));
        source = EmbossTextSource{text, *font};
      } else if (ui->source_kind->currentIndex() == 1)
        source = take(ProfileRegionReference::create(
            sketch, ProfileId(ui->profile->currentData().toString().toStdString())));
      else
        throw std::runtime_error("Emboss/Engrave requires a closed profile or text.");
      std::vector<FaceReference> supports;
      for (int i = 0; i < ui->supports->count(); ++i) {
        auto at = face_references.find(
            ui->supports->item(i)->data(Qt::UserRole).toString().toStdString());
        if (at == face_references.end())
          throw std::runtime_error(
              "A selected support is unavailable. Select its replacement explicitly.");
        supports.push_back(role(at->second, PartFeatureInputRole::SurfaceSourceFace));
      }
      EmbossIntent intent{
          *source,
          supports,
          ui->mode->currentIndex() == 2 ? EmbossOperation::Raise : EmbossOperation::Recess,
          size,
          static_cast<EmbossMapping>(ui->mapping->currentIndex()),
          {number(ui->anchor_x), number(ui->anchor_y)},
          number(ui->seam),
          ui->orientation_enabled->isChecked()
              ? std::optional<ParameterId>(take(parameter(p, ui->orientation, "orientation")))
              : std::nullopt};
      record = take(SketchDrivenFeature::create(feature, ui->name->text().trimmed().toStdString(),
                                                body, SketchDrivenDefinition{intent}));
    }
    take(editing ? p.update_sketch_driven_feature(*record) : p.add_sketch_driven_feature(*record));
    if (!editing && window.session().history_prefix_count())
      take(window.session().prepare_part_history_candidate(p));
    return Result<PartDocument>::success(std::move(p));
  } catch (const std::exception& e) {
    return Result<PartDocument>::failure(error(e.what()));
  }
}
} // namespace blcad::gui
namespace blcad::gui {
void ShellSketchDrivenTools::Impl::load(const SketchDrivenFeature* record) {
  const auto& part = *window.session().part_document();
  original_text.reset();
  ui->body->clear();
  ui->limit_body->clear();
  ui->sketch->clear();
  ui->curves->clear();
  ui->supports->clear();
  ui->tokens->setRowCount(0);
  face_references.clear();
  for (const auto& body : part.bodies())
    if (body.kind() == BodyKind::Solid && input_cache->find_body_shape(body.id())) {
      ui->body->addItem(q(body.name()), q(body.id().value()));
      ui->limit_body->addItem(q(body.name()), q(body.id().value()));
    }
  for (const auto& sketch : part.sketches())
    ui->sketch->addItem(q(sketch.name()), q(sketch.id().value()));
  for (auto* binding : ui->bindings())
    bind(*binding, std::nullopt);
  if (record)
    ui->body->setCurrentIndex(ui->body->findData(q(record->target_body().value())));
  ui->body->setEnabled(!record);
  reload_sources(true);
  reload_targets();
  if (!record) {
    ui->name->setText(ui->mode->currentText() + " " + QString::number(index));
    return;
  }
  ui->name->setText(q(record->name()));
  const auto select = [](QComboBox* field, const std::string& value) {
    int at = field->findData(q(value));
    if (at >= 0)
      field->setCurrentIndex(at);
  };
  if (const auto* r = std::get_if<RibWebIntent>(&record->definition().intent)) {
    ui->mode->setCurrentIndex(r->kind == RibWebKind::Rib ? 0 : 1);
    select(ui->sketch, r->source.sketch.value());
    reload_sources(false);
    ui->source_kind->setCurrentIndex(r->source.profile ? 1 : 0);
    if (r->source.profile)
      select(ui->profile, r->source.profile->value());
    for (const auto& id : r->source.curves) {
      int at = ui->curve->findData(q(id.value()));
      auto* row = new QListWidgetItem(
          at >= 0 ? ui->curve->itemText(at) : "Missing curve: " + q(id.value()), ui->curves);
      row->setData(Qt::UserRole, q(id.value()));
    }
    ui->plane->setCurrentIndex(r->plane == RibExtrusionPlane::PerpendicularToSketch ? 0 : 1);
    ui->side->setCurrentIndex(int(r->side));
    for (int i = 0; i < 3; ++i)
      ui->direction[i]->setText(QString::number(i == 0   ? r->direction.x
                                                : i == 1 ? r->direction.y
                                                         : r->direction.z,
                                                'g', 17));
    bind(ui->size, r->thickness);
    bind(ui->second, r->second_thickness);
    bind(ui->draft, r->draft);
    ui->draft_enabled->setChecked(r->draft.has_value());
    if (const auto* d = std::get_if<RibDistanceExtent>(&r->extent)) {
      ui->extent->setCurrentIndex(0);
      bind(ui->distance, d->distance);
    }
    if (const auto* b = std::get_if<RibBodyExtent>(&r->extent)) {
      ui->extent->setCurrentIndex(2);
      select(ui->limit_body, b->body.value());
    }
    if (const auto* f = std::get_if<RibFaceExtent>(&r->extent)) {
      ui->extent->setCurrentIndex(1);
      const auto key = interactive_face_identity(f->face);
      face_references.insert_or_assign(key, f->face);
      if (ui->limit_face->findData(q(key)) < 0)
        ui->limit_face->addItem("Unavailable selected face", q(key));
      select(ui->limit_face, key);
    }
    ui->crossings->setCurrentIndex(r->crossings == RibCrossingPolicy::Union ? 0 : 1);
  } else {
    const auto& e = std::get<EmbossIntent>(record->definition().intent);
    ui->mode->setCurrentIndex(e.operation == EmbossOperation::Raise ? 2 : 3);
    select(ui->sketch, emboss_source_sketch(e).value());
    reload_sources(false);
    bind(ui->size, e.depth);
    bind(ui->orientation, e.orientation);
    ui->orientation_enabled->setChecked(e.orientation.has_value());
    ui->mapping->setCurrentIndex(int(e.mapping));
    ui->anchor_x->setText(QString::number(e.anchor_mm.x, 'g', 17));
    ui->anchor_y->setText(QString::number(e.anchor_mm.y, 'g', 17));
    ui->seam->setText(QString::number(e.seam_degrees, 'g', 17));
    if (const auto* profile = std::get_if<ProfileRegionReference>(&e.source)) {
      ui->source_kind->setCurrentIndex(1);
      select(ui->profile, profile->profile().value());
    } else {
      const auto& text = std::get<EmbossTextSource>(e.source);
      original_text = text;
      ui->source_kind->setCurrentIndex(2);
      ui->text->setText(q(text.text.template_text()));
      ui->font->setCurrentText(q(text.text.requested_font()));
      bind(ui->height, text.text.height_parameter());
      for (const auto& token : text.text.bindings())
        add_token(&token);
    }
    for (const auto& face : e.supports) {
      const auto key = interactive_face_identity(face);
      face_references.insert_or_assign(key, face);
      int at = ui->face->findData(q(key));
      auto* item = new QListWidgetItem(
          at >= 0 ? ui->face->itemText(at) : "Unavailable selected support", ui->supports);
      item->setData(Qt::UserRole, q(key));
    }
  }
}
} // namespace blcad::gui
