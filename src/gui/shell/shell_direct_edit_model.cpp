#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "direct_edit_tools_impl.hpp"
#include <QSignalBlocker>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace blcad::gui {
namespace {
Error error(const std::string& text) {
  return Error::validation("part.direct_edit", text);
}
template <class T> T take(Result<T> value) {
  if (!value)
    throw std::runtime_error(value.error().message());
  return std::move(value.value());
}
QString q(const std::string& value) {
  return QString::fromStdString(value);
}
double number(const QLineEdit* field) {
  const auto text = field->text().trimmed().toStdString();
  double out;
  auto r = std::from_chars(text.data(), text.data() + text.size(), out);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || !std::isfinite(out))
    throw std::runtime_error("Enter a finite number using a decimal point.");
  return out;
}
Vector3 vector(const std::array<QLineEdit*, 3>& fields) {
  return {number(fields[0]), number(fields[1]), number(fields[2])};
}
void display(const std::array<QLineEdit*, 3>& fields, Vector3 value) {
  const double values[]{value.x, value.y, value.z};
  for (int i = 0; i < 3; ++i)
    fields[i]->setText(QString::number(values[i], 'g', 17));
}
} // namespace

void ShellDirectEditTools::Impl::load_binding(Binding& b, std::optional<ParameterId> id,
                                              double sign) {
  b.parameter->clear();
  b.parameter->addItem("New value", "");
  for (const auto& p : window.session().part_document()->parameters())
    if (p.type() == b.type)
      b.parameter->addItem(q(p.name()), q(p.id().value()));
  if (id) {
    b.parameter->setCurrentIndex(b.parameter->findData(q(id->value())));
    select_binding(b);
    b.value->setText(QString::number(sign * b.value->text().toDouble(), 'g', 17));
  } else
    b.value->setReadOnly(false);
}
void ShellDirectEditTools::Impl::select_binding(Binding& b) {
  const auto* p = window.session().part_document()->find_parameter(
      ParameterId(b.parameter->currentData().toString().toStdString()));
  b.value->setReadOnly(p && p->is_expression());
  if (p)
    b.value->setText(QString::number(
        b.type == ParameterType::Angle ? p->value().degrees() : p->value().millimeters(), 'g', 17));
}
void ShellDirectEditTools::Impl::reload_faces() {
  if (!inputs || !cache)
    return;
  const QSignalBlocker blocker(face);
  face->clear();
  targets.clear();
  const BodyId id(body->currentData().toString().toStdString());
  auto result = query_with_step_import_context(window.session(), [&](auto* context) {
    return geometry::SketchDrivenGuideQuery::faces(*inputs, *cache, id, context, prefix);
  });
  if (!result) {
    fail(result.error());
    return;
  }
  for (auto& t : result.value()) {
    const auto& ref = std::get<FaceReference>(t.selection.reference);
    const auto key = interactive_face_identity(ref);
    references.insert_or_assign(key, ref);
    face->addItem(q(t.label), q(key));
    targets.push_back(std::move(t));
  }
  if (active) {
    std::vector<geometry::ViewportSceneItem> picks;
    for (const auto& target : targets)
      picks.push_back({geometry::ViewportSceneKind::Face, target.selection.semantic_id,
                       target.selection.shape});
    (void)window.viewport().set_preview_selection_targets(std::move(picks));
  }
}
void ShellDirectEditTools::Impl::add_face() {
  if (!current() || face->currentIndex() < 0)
    return;
  const auto key = face->currentData();
  for (int row = 0; row < selected->count(); ++row)
    if (selected->item(row)->data(Qt::UserRole) == key) {
      fail(error("This face is already selected."));
      return;
    }
  auto* row = new QListWidgetItem(face->currentText(), selected);
  row->setData(Qt::UserRole, key);
  changed();
}
DirectEditFrame ShellDirectEditTools::Impl::frame() const {
  const auto point = vector(origin);
  return {{point.x, point.y, point.z}, vector(frame_x), vector(frame_y)};
}
void ShellDirectEditTools::Impl::frame_from_face(bool world_axes) {
  loading = true;
  if (world_axes) {
    display(origin, {});
    display(frame_x, {1, 0, 0});
    display(frame_y, {0, 1, 0});
  } else {
    const auto key =
        selected->count() ? selected->item(0)->data(Qt::UserRole) : face->currentData();
    for (const auto& t : targets)
      if (q(interactive_face_identity(std::get<FaceReference>(t.selection.reference))) ==
          key.toString()) {
        const Vector3 z = normalized(t.normal);
        const Vector3 x =
            normalized(cross(std::abs(z.x) < 0.8 ? Vector3{1, 0, 0} : Vector3{0, 1, 0}, z));
        display(origin, {t.anchor.x, t.anchor.y, t.anchor.z});
        display(frame_x, x);
        display(frame_y, cross(z, x));
        break;
      }
  }
  loading = false;
  changed();
}
Result<ParameterId> ShellDirectEditTools::Impl::parameter(PartDocument& p, const Binding& b,
                                                          const char* suffix) const {
  try {
    double v = number(b.value);
    const auto key = b.parameter->currentData().toString().toStdString();
    const std::string_view component(suffix);
    const bool zero_component = v == 0 && b.type == ParameterType::Length &&
                                (component == "x" || component == "y" || component == "z");
    const auto* bound = key.empty() ? nullptr : p.find_parameter(ParameterId(key));
    if (!key.empty() && (!bound || bound->type() != b.type))
      throw std::runtime_error("The selected parameter is unavailable.");
    if (zero_component && bound)
      return Result<ParameterId>::success(bound->id());
    if (b.type == ParameterType::Length) {
      v = std::abs(v);
      if (zero_component)
        v = 1e-6;
    }
    const auto quantity =
        take(b.type == ParameterType::Angle ? Quantity::angle_deg(v) : Quantity::length_mm(v));
    if (!key.empty()) {
      const auto* old = bound;
      const double prior =
          b.type == ParameterType::Angle ? old->value().degrees() : old->value().millimeters();
      if (v != prior) {
        if (old->is_expression())
          throw std::runtime_error(
              "Formula values are read-only. Choose New value to rebind explicitly.");
        take(p.set_parameter_value(old->id(), quantity));
      }
      return Result<ParameterId>::success(ParameterId(key));
    }
    ParameterId id(feature.value() + "." + suffix);
    std::string label = name->text().toStdString() + " " + suffix;
    const auto base = label;
    for (std::size_t i = 1; p.find_parameter(id) || p.find_parameter(std::string_view(label));
         ++i) {
      id = ParameterId(feature.value() + "." + suffix + "." + std::to_string(i));
      label = base + " " + std::to_string(i);
    }
    take(p.add_parameter(take(b.type == ParameterType::Angle
                                  ? Parameter::create_angle(id, label, quantity)
                                  : Parameter::create_length(id, label, quantity))));
    return Result<ParameterId>::success(id);
  } catch (const std::exception& e) {
    return Result<ParameterId>::failure(error(e.what()));
  }
}
Result<PartDocument> ShellDirectEditTools::Impl::candidate() const {
  try {
    if (!current())
      throw std::runtime_error("The model changed. Reopen Direct Edit.");
    auto p = *window.session().part_document();
    const BodyId target(body->currentData().toString().toStdString());
    const auto* source = cache->find_body_result(target);
    if (!source)
      throw std::runtime_error("Selected target Body has no current predecessor.");
    std::vector<DirectEditFace> selections;
    for (int row = 0; row < selected->count(); ++row) {
      const auto key = selected->item(row)->data(Qt::UserRole).toString().toStdString();
      auto ref = references.find(key);
      auto t = std::find_if(targets.begin(), targets.end(), [&](const auto& face) {
        return interactive_face_identity(std::get<FaceReference>(face.selection.reference)) == key;
      });
      if (ref == references.end() || t == targets.end())
        throw std::runtime_error(
            "A selected face is missing. Remove it and select its replacement explicitly.");
      const auto expected = t->mapping == EmbossMapping::Planar ? DirectEditFaceCapability::Planar
                            : t->mapping == EmbossMapping::Cylindrical
                                ? DirectEditFaceCapability::Cylindrical
                                : DirectEditFaceCapability::AnySurface;
      selections.push_back({ref->second, expected});
    }
    DirectEditOperation op = DirectFaceDeleteHeal{};
    const auto sign = [](double v) { return v < 0 ? -1.0 : v > 0 ? 1.0 : 0.0; };
    if (mode->currentIndex() == 0)
      op = DirectFaceMove{
          take(parameter(p, move[0], "x")),
          take(parameter(p, move[1], "y")),
          take(parameter(p, move[2], "z")),
          {sign(number(move[0].value)), sign(number(move[1].value)), sign(number(move[2].value))}};
    if (mode->currentIndex() == 1)
      op = DirectFaceRotate{vector(axis), take(parameter(p, angle, "angle"))};
    if (mode->currentIndex() == 2)
      op = DirectFaceOffset{take(parameter(p, size, "offset")), number(size.value) < 0};
    if (mode->currentIndex() == 3) {
      if (number(size.value) <= 0)
        throw std::runtime_error("Radius/diameter must be positive.");
      op = DirectFaceSize{take(parameter(p, size, "size")), measurement->currentIndex() == 0
                                                                ? DirectEditSizeMode::Radius
                                                                : DirectEditSizeMode::Diameter};
    }
    auto record = take(DirectEditFeature::create(feature, name->text().trimmed().toStdString(),
                                                 target, source->source_feature_id, selections, op,
                                                 frame(), {number(tolerance)}));
    take(editing ? p.update_direct_edit_feature(record) : p.add_direct_edit_feature(record));
    if (!editing && window.session().history_prefix_count())
      take(window.session().prepare_part_history_candidate(p));
    return Result<PartDocument>::success(std::move(p));
  } catch (const std::exception& e) {
    return Result<PartDocument>::failure(error(e.what()));
  }
}
void ShellDirectEditTools::Impl::load(const DirectEditFeature* record) {
  const auto& p = *window.session().part_document();
  body->clear();
  selected->clear();
  references.clear();
  for (const auto& b : p.bodies())
    if (b.kind() == BodyKind::Solid && cache->find_body_shape(b.id()))
      body->addItem(q(b.name()), q(b.id().value()));
  for (auto& b : move)
    load_binding(b, std::nullopt);
  load_binding(angle, std::nullopt);
  load_binding(size, std::nullopt);
  if (record)
    body->setCurrentIndex(body->findData(q(record->target_body().value())));
  body->setEnabled(!record);
  reload_faces();
  if (!record) {
    name->setText("Direct Edit " + q(feature.value()));
    return;
  }
  name->setText(q(record->name()));
  tolerance->setText(QString::number(record->policy().tolerance_mm, 'g', 17));
  const auto& f = record->frame();
  display(origin, {f.origin_mm.x, f.origin_mm.y, f.origin_mm.z});
  display(frame_x, f.x_axis);
  display(frame_y, f.y_axis);
  for (const auto& selection : record->selections()) {
    const auto key = interactive_face_identity(selection.reference);
    references.insert_or_assign(key, selection.reference);
    int index = face->findData(q(key));
    auto* item = new QListWidgetItem(index >= 0 ? face->itemText(index)
                                                : "Missing selected face " +
                                                      QString::number(selected->count() + 1),
                                     selected);
    item->setData(Qt::UserRole, q(key));
  }
  std::visit(
      [&](const auto& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, DirectFaceMove>) {
          mode->setCurrentIndex(0);
          load_binding(move[0], op.x, op.signs.x);
          load_binding(move[1], op.y, op.signs.y);
          load_binding(move[2], op.z, op.signs.z);
        } else if constexpr (std::is_same_v<T, DirectFaceRotate>) {
          mode->setCurrentIndex(1);
          load_binding(angle, op.angle);
          display(axis, op.axis);
        } else if constexpr (std::is_same_v<T, DirectFaceOffset>) {
          mode->setCurrentIndex(2);
          load_binding(size, op.distance, op.reverse ? -1 : 1);
        } else if constexpr (std::is_same_v<T, DirectFaceSize>) {
          mode->setCurrentIndex(3);
          load_binding(size, op.size);
          measurement->setCurrentIndex(op.mode == DirectEditSizeMode::Radius ? 0 : 1);
        } else
          mode->setCurrentIndex(4);
      },
      record->operation());
}
} // namespace blcad::gui
