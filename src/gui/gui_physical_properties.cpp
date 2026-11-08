#include "blcad/gui/gui_physical_properties.hpp"
#include "blcad/gui/gui_step_import_context.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace blcad::gui {
namespace {
using namespace geometry;
Error error(std::string message) {
  return Error::validation("gui.physical_properties", std::move(message));
}
bool ready(const GuiDocumentSession& s) {
  return s.has_document() && s.derived_results_fresh() && !s.part_workflow_active() &&
         (!s.task().active() || s.task().command_id() == GuiPhysicalPropertiesController::task_id);
}
bool same_occurrence(const AssemblyLeafOccurrenceDescriptor& a,
                     const AssemblyLeafOccurrenceDescriptor& b) {
  return a.assembly_document == b.assembly_document && a.subassembly_path == b.subassembly_path &&
         a.component_instance == b.component_instance &&
         a.referenced_part_document == b.referenced_part_document;
}
std::string occurrence_label(const Project& project,
                             const PhysicalOccurrenceProperties& occurrence) {
  const AssemblyDocument* a = &project.assembly();
  std::string name = a->name();
  for (const auto& id : occurrence.occurrence.subassembly_path) {
    const auto* instance = a ? a->find_subassembly_instance(id) : nullptr;
    name += " / " + (instance ? instance->name() : id.value());
    a = instance ? project.find_assembly_document(instance->referenced_assembly_document())
                 : nullptr;
  }
  return name + " / " + occurrence.name;
}
PhysicalPropertiesTotals body_totals(const BodyPhysicalProperties& body) {
  PhysicalPropertiesTotals t;
  t.area_mm2 = body.geometry.area_mm2;
  t.estimated_relative_area_error = body.geometry.estimated_relative_area_error;
  t.estimated_relative_volume_error = body.geometry.estimated_relative_volume_error;
  if (body.kind == BodyKind::Surface) {
    t.surface_body_count = 1;
    return t;
  }
  t.solid_body_count = 1;
  t.volume_mm3 = body.geometry.volume->volume_mm3;
  t.volume_centroid_mm = body.geometry.volume->centroid_mm;
  t.known_mass = body.mass;
  t.mass_status = body.mass ? PhysicalMassStatus::Complete : PhysicalMassStatus::Unavailable;
  t.missing_density_body_count = body.mass ? 0 : 1;
  return t;
}
double convert(double value, long double factor) {
  const long double scaled = value * factor;
  const double result = static_cast<double>(scaled);
  if (!std::isfinite(scaled) || !std::isfinite(result) || (value != 0 && result == 0))
    throw std::runtime_error("Physical value exceeds the chosen display unit's numeric range");
  return result == 0 ? 0 : result;
}
Point3 convert(Point3 p, long double factor) {
  return {convert(p.x, factor), convert(p.y, factor), convert(p.z, factor)};
}
std::string number(double value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(12) << value;
  return out.str();
}
std::string point(Point3 p) {
  return "(" + number(p.x) + ", " + number(p.y) + ", " + number(p.z) + ")";
}
} // namespace

Result<std::size_t> GuiPhysicalPropertiesController::begin(const GuiDocumentSession& s) {
  if (active_ || !ready(s))
    return Result<std::size_t>::failure(
        error("Physical Properties requires complete fresh geometry and an idle command context"));
  active_ = true;
  current_ = false;
  geometry_reused_ = false;
  scope_ = {};
  return Result<std::size_t>::success(1);
}
Result<std::size_t> GuiPhysicalPropertiesController::refresh(const GuiDocumentSession& s) {
  current_ = false;
  geometry_reused_ = false;
  if (!active_ || !ready(s))
    return Result<std::size_t>::failure(
        error("Finish the active workflow and recompute before Physical Properties"));
  return query_with_step_import_context(
      s, [&](StepImportTopologyContext* imports) -> Result<std::size_t> {
        const BodyResultInspector inspector;
        if (const auto* p = s.part_document()) {
          const auto* old = std::get_if<PartPhysicalProperties>(&snapshot_);
          auto result = old ? inspector.inspect_physical_properties(*p, *old, imports)
                            : inspector.inspect_physical_properties(*p, imports);
          if (!result && old)
            result = inspector.inspect_physical_properties(*p, imports);
          if (!result)
            return Result<std::size_t>::failure(result.error());
          geometry_reused_ = result.value().data().geometry_reused;
          snapshot_ = std::move(result.value());
        } else {
          const auto* old = std::get_if<ProjectPhysicalProperties>(&snapshot_);
          auto result = old ? inspector.inspect_physical_properties(*s.project(), *old, imports)
                            : inspector.inspect_physical_properties(*s.project(), imports);
          if (result && old)
            geometry_reused_ = true;
          if (!result && old)
            result = inspector.inspect_physical_properties(*s.project(), imports);
          if (!result)
            return Result<std::size_t>::failure(result.error());
          snapshot_ = std::move(result.value());
        }
        revision_ = s.model_revision();
        current_ = true;
        const auto chosen = selected(s, scope_);
        if (!chosen)
          return Result<std::size_t>::failure(chosen.error());
        return Result<std::size_t>::success(chosen.value().bodies.size());
      });
}
Result<std::size_t> GuiPhysicalPropertiesController::validate(const GuiDocumentSession& s) const {
  if (!active_ || !current_ || !ready(s) || revision_ != s.model_revision())
    return Result<std::size_t>::failure(
        error("Physical Properties is stale; refresh the current model"));
  return query_with_step_import_context(
      s, [&](StepImportTopologyContext* imports) -> Result<std::size_t> {
        if (const auto* p = std::get_if<PartPhysicalProperties>(&snapshot_); p && s.part_document())
          return p->validate_current(*s.part_document(), imports);
        if (const auto* p = std::get_if<ProjectPhysicalProperties>(&snapshot_); p && s.project())
          return p->validate_current(*s.project(), imports);
        return Result<std::size_t>::failure(error("The physical document context changed"));
      });
}
Result<GuiPhysicalPropertiesController::Selection>
GuiPhysicalPropertiesController::selected(const GuiDocumentSession& s,
                                          const GuiPhysicalScope& scope) const {
  Selection choice;
  if (const auto* p = std::get_if<PartPhysicalProperties>(&snapshot_); p && s.part_document()) {
    if (scope.occurrence)
      return Result<Selection>::failure(error("Part scope cannot identify an assembly occurrence"));
    choice.totals = p->data();
    choice.label = "Part / " + s.part_document()->name();
    for (const auto& body : p->data().bodies)
      choice.bodies.push_back(&body);
  } else if (const auto* p = std::get_if<ProjectPhysicalProperties>(&snapshot_); p && s.project()) {
    choice.totals = p->data();
    choice.label = "Root Assembly / " + s.project()->assembly().name();
    if (scope.occurrence) {
      const auto& rows = p->data().occurrences;
      const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& item) {
        return same_occurrence(item.occurrence, *scope.occurrence);
      });
      if (row == rows.end())
        return Result<Selection>::failure(
            error("The physical occurrence is unavailable; choose a current scope"));
      choice.totals = *row;
      choice.label = occurrence_label(*s.project(), *row);
      for (const auto& body : row->bodies)
        choice.bodies.push_back(&body);
    } else {
      if (scope.body)
        return Result<Selection>::failure(
            error("An assembly Body scope requires its exact occurrence"));
      for (const auto& row : p->data().occurrences)
        for (const auto& body : row.bodies)
          choice.bodies.push_back(&body);
    }
  } else
    return Result<Selection>::failure(error("No physical snapshot is available"));
  if (scope.body) {
    const auto found = std::find_if(choice.bodies.begin(), choice.bodies.end(),
                                    [&](const auto* b) { return b->body == scope.body; });
    if (found == choice.bodies.end())
      return Result<Selection>::failure(
          error("The physical Body is unavailable; choose a current scope"));
    const auto* body = *found;
    choice.totals = body_totals(*body);
    choice.label += " / " + body->name;
    choice.bodies = {body};
  }
  return Result<Selection>::success(std::move(choice));
}
Result<std::size_t> GuiPhysicalPropertiesController::select_scope(const GuiDocumentSession& s,
                                                                  GuiPhysicalScope scope) {
  const auto valid = validate(s);
  if (!valid)
    return valid;
  const auto chosen = selected(s, scope);
  if (!chosen)
    return Result<std::size_t>::failure(chosen.error());
  scope_ = std::move(scope);
  return Result<std::size_t>::success(chosen.value().bodies.size());
}
Result<std::vector<GuiPhysicalScopeChoice>>
GuiPhysicalPropertiesController::scopes(const GuiDocumentSession& s) const {
  const auto valid = validate(s);
  if (!valid)
    return Result<std::vector<GuiPhysicalScopeChoice>>::failure(valid.error());
  std::vector<GuiPhysicalScopeChoice> choices;
  choices.push_back({{}, s.part_document() ? "Whole Part" : "Root Assembly"});
  if (const auto* p = std::get_if<PartPhysicalProperties>(&snapshot_)) {
    for (const auto& body : p->data().bodies)
      if (body.body)
        choices.push_back({{body.body, {}}, body.name});
  } else {
    for (const auto& row : std::get<ProjectPhysicalProperties>(snapshot_).data().occurrences) {
      const auto label = occurrence_label(*s.project(), row);
      choices.push_back({{{}, row.occurrence}, label});
      for (const auto& body : row.bodies)
        if (body.body)
          choices.push_back({{body.body, row.occurrence}, label + " / " + body.name});
    }
  }
  return Result<std::vector<GuiPhysicalScopeChoice>>::success(std::move(choices));
}
Result<std::size_t> GuiPhysicalPropertiesController::set_units(GuiPhysicalUnits units) {
  if ((units.length != GuiPhysicalLengthUnit::Millimeter &&
       units.length != GuiPhysicalLengthUnit::Centimeter &&
       units.length != GuiPhysicalLengthUnit::Meter) ||
      (units.mass != GuiPhysicalMassUnit::Kilogram && units.mass != GuiPhysicalMassUnit::Gram))
    return Result<std::size_t>::failure(error("Choose supported physical display units"));
  units_ = units;
  return Result<std::size_t>::success(1);
}
Result<GuiPhysicalPropertiesView>
GuiPhysicalPropertiesController::view(const GuiDocumentSession& s) const {
  const auto valid = validate(s);
  if (!valid)
    return Result<GuiPhysicalPropertiesView>::failure(valid.error());
  const auto chosen = selected(s, scope_);
  if (!chosen)
    return Result<GuiPhysicalPropertiesView>::failure(chosen.error());
  try {
    const auto& totals = chosen.value().totals;
    const long double length = units_.length == GuiPhysicalLengthUnit::Millimeter   ? 1
                               : units_.length == GuiPhysicalLengthUnit::Centimeter ? .1L
                                                                                    : .001L;
    const long double mass = units_.mass == GuiPhysicalMassUnit::Kilogram ? 1 : 1000;
    GuiPhysicalPropertiesView out;
    out.canonical = totals;
    out.scope_label = chosen.value().label;
    out.coordinate_frame = s.part_document() ? "Part coordinates" : "Root assembly coordinates";
    out.length_unit = units_.length == GuiPhysicalLengthUnit::Millimeter   ? "mm"
                      : units_.length == GuiPhysicalLengthUnit::Centimeter ? "cm"
                                                                           : "m";
    out.mass_unit = units_.mass == GuiPhysicalMassUnit::Kilogram ? "kg" : "g";
    out.area_unit = out.length_unit + "²";
    out.volume_unit = out.length_unit + "³";
    out.inertia_unit = out.mass_unit + "·" + out.area_unit;
    out.density_unit = out.mass_unit + "/" + out.volume_unit;
    out.area = convert(totals.area_mm2, length * length);
    out.volume = convert(totals.volume_mm3, length * length * length);
    if (totals.volume_centroid_mm)
      out.volume_centroid = convert(*totals.volume_centroid_mm, length);
    out.mass_status = totals.mass_status;
    out.solid_body_count = totals.solid_body_count;
    out.surface_body_count = totals.surface_body_count;
    out.missing_density_body_count = totals.missing_density_body_count;
    out.estimated_relative_area_error = totals.estimated_relative_area_error;
    out.estimated_relative_volume_error = totals.estimated_relative_volume_error;
    if (totals.known_mass) {
      const auto& m = *totals.known_mass;
      GuiPhysicalMassView v;
      v.mass = convert(m.mass_kg, mass);
      v.centroid = convert(m.centroid_mm, length);
      for (std::size_t i = 0; i < 9; ++i)
        v.central_inertia[i] = convert(m.central_inertia_kg_mm2[i], mass * length * length);
      for (std::size_t i = 0; i < 3; ++i)
        v.principal_moments[i] = convert(m.principal_moments_kg_mm2[i], mass * length * length);
      v.principal_directions = m.principal_directions;
      v.principal_axes_unique = m.principal_axes_unique;
      out.known_mass = v;
    }
    for (const auto* body : chosen.value().bodies) {
      if (!body->material) {
        ++out.unassigned_body_count;
        continue;
      }
      const auto found =
          std::find_if(out.materials.begin(), out.materials.end(),
                       [&](const auto& item) { return item.key == *body->material; });
      if (found != out.materials.end()) {
        ++found->body_count;
        continue;
      }
      const auto& catalog = s.part_document() ? s.part_document()->engineering_resources()
                                              : s.project()->engineering_resources();
      const auto* material = catalog.find_material(*body->material);
      if (!material)
        return Result<GuiPhysicalPropertiesView>::failure(
            error("A physical material snapshot is unavailable"));
      std::optional<double> density;
      if (material->properties().density)
        density = convert(material->properties().density->value.kg_per_m3(),
                          1e-9L * mass / (length * length * length));
      out.materials.push_back({material->key(), material->name(), density, 1});
    }
    return Result<GuiPhysicalPropertiesView>::success(std::move(out));
  } catch (const std::exception& e) {
    return Result<GuiPhysicalPropertiesView>::failure(error(e.what()));
  }
}
Result<std::string>
GuiPhysicalPropertiesController::clipboard_text(const GuiDocumentSession& s) const {
  const auto value = view(s);
  if (!value)
    return Result<std::string>::failure(value.error());
  const auto& v = value.value();
  std::string out =
      "Physical Properties\nScope: " + v.scope_label + "\nFrame: " + v.coordinate_frame + "\n";
  out += "Solid Bodies: " + std::to_string(v.solid_body_count) +
         "\nSurface Bodies: " + std::to_string(v.surface_body_count) + "\n";
  out += "Area: " + number(v.area) + " " + v.area_unit + "\n";
  out += v.solid_body_count ? "Solid volume: " + number(v.volume) + " " + v.volume_unit + "\n"
                            : "Solid volume: Not applicable (no solid Bodies)\n";
  if (v.volume_centroid)
    out += "Volume centroid: " + point(*v.volume_centroid) + " " + v.length_unit + "\n";
  const auto status =
      v.mass_status == PhysicalMassStatus::Complete      ? "Complete"
      : v.mass_status == PhysicalMassStatus::Partial     ? "Partial (known-density subset)"
      : v.mass_status == PhysicalMassStatus::Unavailable ? "Unavailable (no known density)"
                                                         : "Not applicable (no solid Bodies)";
  out += "Mass status: " + std::string(status) + "\n";
  if (v.known_mass) {
    const auto& m = *v.known_mass;
    const std::string prefix = v.mass_status == PhysicalMassStatus::Partial ? "Known-subset " : "";
    out += prefix + "Mass: " + number(m.mass) + " " + v.mass_unit + "\n";
    out += prefix + "Mass centroid: " + point(m.centroid) + " " + v.length_unit + "\n";
    out += prefix + "Central inertia [" + v.inertia_unit + "]:\n";
    for (std::size_t i = 0; i < 3; ++i)
      out += number(m.central_inertia[3 * i]) + "\t" + number(m.central_inertia[3 * i + 1]) + "\t" +
             number(m.central_inertia[3 * i + 2]) + "\n";
    out += prefix + "Principal moments [" + v.inertia_unit +
           "]: " + number(m.principal_moments[0]) + ", " + number(m.principal_moments[1]) + ", " +
           number(m.principal_moments[2]) + "\n";
    out += prefix + "Principal axes: " +
           (m.principal_axes_unique ? "unique up to direction sign"
                                    : "not unique (repeated or near-repeated moments)") +
           "\n";
    if (m.principal_axes_unique)
      for (std::size_t i = 0; i < 3; ++i) {
        const auto d = m.principal_directions[i];
        out += prefix + "Principal direction " + std::to_string(i + 1) +
               " (unitless): " + point({d.x, d.y, d.z}) + "\n";
      }
  }
  out += "Solid Bodies without known density: " + std::to_string(v.missing_density_body_count) +
         "\nUnassigned Bodies: " + std::to_string(v.unassigned_body_count) + "\n";
  for (const auto& material : v.materials) {
    out += "Material: " + material.name + " @ " + material.key.revision() + " (" +
           std::to_string(material.body_count) + " Bodies)";
    out += material.density ? ", density: " + number(*material.density) + " " + v.density_unit
                            : ", density unavailable";
    out += "\n";
  }
  out += "Requested relative integration tolerance: " + number(v.requested_relative_precision) +
         "\nEstimated relative area/volume error: " + number(v.estimated_relative_area_error) +
         " / " + number(v.estimated_relative_volume_error) + "\n";
  return Result<std::string>::success(std::move(out));
}
void GuiPhysicalPropertiesController::cancel() noexcept {
  // Retain only immutable geometry for the next guarded refresh; inactive data
  // cannot be presented/copied. Unit preference is transient for this controller.
  active_ = false;
  current_ = false;
  geometry_reused_ = false;
  scope_ = {};
  revision_ = 0;
}
} // namespace blcad::gui
