#include "blcad/geometry/step_import_topology.hpp"

#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/occt_shape_view.hpp"

#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomTools.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace blcad::geometry {
namespace {

constexpr double kLengthGrid = 1e-7;
constexpr double kDirectionGrid = 1e-10;
constexpr double kMatchTolerance = 1e-6;
constexpr const char* kId = "geometry.step_import_topology";

void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}

Error invalid(std::string message) {
  return Error::geometry(kId, std::move(message));
}

std::string hash(std::string_view value) {
  auto result = step_import_sha256(value);
  if (!result)
    throw std::runtime_error(result.error().message());
  return result.value();
}

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  result << std::setprecision(std::numeric_limits<double>::max_digits10);
  return result;
}

void number(std::ostream& stream, double value, double grid = kLengthGrid) {
  require(std::isfinite(value), "imported topology evidence must be finite");
  const double units = value / grid;
  const double normalized = std::isfinite(units) ? std::round(units) * grid : value;
  stream << (normalized == 0 ? 0.0 : normalized) << ',';
}

void point(std::ostream& stream, Point3 value) {
  number(stream, value.x);
  number(stream, value.y);
  number(stream, value.z);
}

void direction(std::ostream& stream, Vector3 value) {
  number(stream, value.x, kDirectionGrid);
  number(stream, value.y, kDirectionGrid);
  number(stream, value.z, kDirectionGrid);
}

Point3 point(const gp_Pnt& value) {
  return {value.X(), value.Y(), value.Z()};
}

Vector3 canonical(const gp_Dir& value) {
  Vector3 result{value.X(), value.Y(), value.Z()};
  if (std::abs(result.x) < 1e-12)
    result.x = 0;
  if (std::abs(result.y) < 1e-12)
    result.y = 0;
  if (std::abs(result.z) < 1e-12)
    result.z = 0;
  const double magnitude = std::hypot(result.x, result.y, result.z);
  require(std::isfinite(magnitude) && magnitude > 0,
          "imported topology has an invalid analytic direction");
  result = {result.x / magnitude, result.y / magnitude, result.z / magnitude};
  const double first = result.x != 0 ? result.x : result.y != 0 ? result.y : result.z;
  if (first < 0)
    result = {-result.x, -result.y, -result.z};
  return result;
}

Point3 closest_axis_origin(Point3 origin, Vector3 axis) {
  const double along = project_onto_axis(origin, axis);
  return {origin.x - along * axis.x, origin.y - along * axis.y, origin.z - along * axis.z};
}

bool less_point(Point3 a, Point3 b) {
  return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
}

// Normalize the exact OCCT equation serialization after applying its placement into physical
// product-local mm. This opaque witness is never used to approximate another analytic class.
std::string normalized_equation(std::string text) {
  static const std::regex numeric(R"([-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)");
  auto stream = output();
  std::size_t cursor = 0;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), numeric);
       it != std::sregex_iterator(); ++it) {
    const auto position = static_cast<std::size_t>(it->position());
    stream << text.substr(cursor, position - cursor);
    std::istringstream value(it->str());
    value.imbue(std::locale::classic());
    double parsed = 0;
    value >> parsed;
    require(!value.fail(), "imported opaque equation contains an invalid numeric value");
    number(stream, parsed);
    cursor = position + static_cast<std::size_t>(it->length());
  }
  stream << text.substr(cursor);
  return hash(stream.str());
}

std::string complete_key(const ImportedTopologyDescriptor& value) {
  auto stream = output();
  stream << static_cast<int>(value.kind) << ':' << static_cast<int>(value.geometry) << ':';
  if (value.body_kind)
    stream << static_cast<int>(*value.body_kind);
  stream << ':' << value.geometry_key << ':';
  point(stream, value.centroid);
  for (double bound : value.bounds_mm)
    number(stream, bound);
  number(stream, value.measure);
  point(stream, value.origin);
  direction(stream, value.direction);
  number(stream, value.radius_mm);
  for (const auto& sample : value.samples)
    point(stream, sample);
  for (const auto& boundary : value.boundary_keys)
    stream << boundary << ';';
  return hash(stream.str());
}

void bounds_and_measure(ImportedTopologyDescriptor& value, const TopoDS_Shape& shape) {
  Bnd_Box bounds;
  BRepBndLib::AddOptimal(shape, bounds, false, false);
  require(!bounds.IsVoid() && !bounds.IsOpen(),
          "imported topology must have finite bounded geometry");
  bounds.Get(value.bounds_mm[0], value.bounds_mm[1], value.bounds_mm[2], value.bounds_mm[3],
             value.bounds_mm[4], value.bounds_mm[5]);
  GProp_GProps measure;
  if (value.kind == ImportedTopologyKind::Vertex) {
    value.centroid = point(BRep_Tool::Pnt(TopoDS::Vertex(shape)));
  } else if (value.kind == ImportedTopologyKind::Edge) {
    BRepGProp::LinearProperties(shape, measure);
    value.centroid = point(measure.CentreOfMass());
    value.measure = measure.Mass();
  } else if (value.kind == ImportedTopologyKind::Body && value.body_kind == BodyKind::Solid) {
    BRepGProp::VolumeProperties(shape, measure);
    value.centroid = point(measure.CentreOfMass());
    value.measure = measure.Mass();
  } else {
    BRepGProp::SurfaceProperties(shape, measure);
    value.centroid = point(measure.CentreOfMass());
    value.measure = measure.Mass();
  }
  require(std::isfinite(value.measure) && value.measure >= 0,
          "imported topology measure must be nonnegative and finite");
}

ImportedTopologyDescriptor edge_descriptor(const TopoDS_Edge& edge) {
  ImportedTopologyDescriptor value;
  value.kind = ImportedTopologyKind::Edge;
  value.geometry = ImportedGeometryClass::Opaque;
  bounds_and_measure(value, edge);
  auto equation = output();
  if (BRep_Tool::Degenerated(edge)) {
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(edge, TopAbs_VERTEX, vertices);
    require(vertices.Extent() > 0, "imported degenerate edge has no bounding vertex");
    value.centroid = point(BRep_Tool::Pnt(TopoDS::Vertex(vertices(1))));
    value.samples.push_back(value.centroid);
    equation << "degenerated-edge:";
    point(equation, value.centroid);
    value.geometry_key = hash(equation.str());
    return value;
  }
  BRepAdaptor_Curve curve(edge);
  const double first = curve.FirstParameter(), last = curve.LastParameter();
  require(std::isfinite(first) && std::isfinite(last), "imported edge has unbounded parameters");
  if (curve.GetType() == GeomAbs_Line) {
    value.geometry = ImportedGeometryClass::Line;
    const auto line = curve.Line();
    value.direction = canonical(line.Direction());
    value.origin = closest_axis_origin(point(line.Location()), value.direction);
    value.samples = {point(curve.Value(first)), point(curve.Value(last))};
  } else if (curve.GetType() == GeomAbs_Circle) {
    value.geometry = ImportedGeometryClass::Circle;
    const auto circle = curve.Circle();
    value.origin = point(circle.Location());
    value.direction = canonical(circle.Axis().Direction());
    value.radius_mm = circle.Radius();
    const bool complete = std::abs(std::abs(last - first) - 2 * std::acos(-1.0)) <= 1e-10;
    equation << (complete ? "complete-circle:" : "trimmed-circle:");
    if (!complete)
      for (int i = 0; i < 17; ++i)
        value.samples.push_back(point(curve.Value(first + (last - first) * i / 16.0)));
  } else {
    double start = 0, end = 0;
    auto physical = BRep_Tool::Curve(edge, start, end);
    if (physical.IsNull()) {
      require(BRep_Tool::Degenerated(edge), "imported edge has no exact curve equation");
      equation << "degenerated-edge:";
    } else {
      auto raw = output();
      GeomTools::Write(physical, raw);
      equation << normalized_equation(raw.str());
      for (int i = 0; i < 17; ++i)
        value.samples.push_back(point(curve.Value(first + (last - first) * i / 16.0)));
    }
  }
  std::sort(value.samples.begin(), value.samples.end(), less_point);
  equation << static_cast<int>(value.geometry) << ':';
  point(equation, value.origin);
  direction(equation, value.direction);
  number(equation, value.radius_mm);
  for (const auto& sample : value.samples)
    point(equation, sample);
  value.geometry_key = hash(equation.str());
  return value;
}

ImportedTopologyDescriptor face_descriptor(const TopoDS_Face& face) {
  ImportedTopologyDescriptor value;
  value.kind = ImportedTopologyKind::Face;
  value.geometry = ImportedGeometryClass::Opaque;
  bounds_and_measure(value, face);
  BRepAdaptor_Surface surface(face, false);
  auto equation = output();
  if (surface.GetType() == GeomAbs_Plane) {
    value.geometry = ImportedGeometryClass::Plane;
    const auto plane = surface.Plane();
    value.direction = canonical(plane.Axis().Direction());
    const double offset = project_onto_axis(point(plane.Location()), value.direction);
    value.origin = {offset * value.direction.x, offset * value.direction.y,
                    offset * value.direction.z};
  } else if (surface.GetType() == GeomAbs_Cylinder) {
    value.geometry = ImportedGeometryClass::Cylinder;
    const auto cylinder = surface.Cylinder();
    value.direction = canonical(cylinder.Axis().Direction());
    value.origin = closest_axis_origin(point(cylinder.Location()), value.direction);
    value.radius_mm = cylinder.Radius();
  } else {
    auto physical = BRep_Tool::Surface(face);
    require(!physical.IsNull(), "imported face has no exact surface equation");
    auto raw = output();
    GeomTools::Write(physical, raw);
    equation << normalized_equation(raw.str());
  }
  equation << static_cast<int>(value.geometry) << ':';
  point(equation, value.origin);
  direction(equation, value.direction);
  number(equation, value.radius_mm);
  value.geometry_key = hash(equation.str());
  // Preserve every complete trimmed wire as an unordered edge multiset. The wire grouping retains
  // holes and disconnected boundaries; no infinite plane/cylinder match can replace this witness.
  for (TopoDS_Iterator it(face, false, true); it.More(); it.Next()) {
    if (it.Value().ShapeType() != TopAbs_WIRE)
      continue;
    std::vector<std::string> edges;
    for (TopoDS_Iterator edge(it.Value(), false, true); edge.More(); edge.Next())
      if (edge.Value().ShapeType() == TopAbs_EDGE)
        edges.push_back(complete_key(edge_descriptor(TopoDS::Edge(edge.Value()))));
    std::sort(edges.begin(), edges.end());
    std::string wire = "wire:";
    for (const auto& key : edges)
      wire += key;
    value.boundary_keys.push_back(hash(wire));
  }
  std::sort(value.boundary_keys.begin(), value.boundary_keys.end());
  require(!value.boundary_keys.empty(), "imported face has no complete trimmed boundary");
  return value;
}

ImportedTopologyDescriptor vertex_descriptor(const TopoDS_Vertex& vertex) {
  ImportedTopologyDescriptor value;
  value.kind = ImportedTopologyKind::Vertex;
  value.geometry = ImportedGeometryClass::Point;
  bounds_and_measure(value, vertex);
  value.origin = value.centroid;
  auto equation = output();
  point(equation, value.origin);
  value.geometry_key = hash(equation.str());
  return value;
}

struct Entity {
  ImportedTopologyDescriptor descriptor;
  TopoDS_Shape shape;
};

struct BodyCandidate {
  std::size_t product_index{}, body_index{};
  Entity body;
  std::vector<Entity> entities;
};

BodyCandidate candidate(const StepImportedBodyDefinition& imported, std::size_t product,
                        std::size_t body) {
  const auto* shape = OcctShapeView::get(imported.shape);
  require(shape != nullptr && !shape->IsNull(), "imported body shape is unavailable");
  BodyCandidate result;
  result.product_index = product;
  result.body_index = body;
  result.body.shape = *shape;
  auto& descriptor = result.body.descriptor;
  descriptor.kind = ImportedTopologyKind::Body;
  descriptor.geometry = ImportedGeometryClass::Opaque;
  descriptor.body_kind = imported.kind;
  bounds_and_measure(descriptor, *shape);
  for (const auto kind : {TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
    TopTools_IndexedMapOfShape topology;
    TopExp::MapShapes(*shape, kind, topology);
    for (int index = 1; index <= topology.Extent(); ++index) {
      ImportedTopologyDescriptor entry =
          kind == TopAbs_FACE   ? face_descriptor(TopoDS::Face(topology(index)))
          : kind == TopAbs_EDGE ? edge_descriptor(TopoDS::Edge(topology(index)))
                                : vertex_descriptor(TopoDS::Vertex(topology(index)));
      if (kind == TopAbs_FACE)
        descriptor.boundary_keys.push_back(complete_key(entry));
      result.entities.push_back({std::move(entry), topology(index)});
    }
  }
  std::sort(descriptor.boundary_keys.begin(), descriptor.boundary_keys.end());
  std::string equation = imported.kind == BodyKind::Solid ? "solid:" : "surface:";
  for (const auto& face : descriptor.boundary_keys)
    equation += face;
  descriptor.geometry_key = hash(equation);
  std::sort(result.entities.begin(), result.entities.end(), [](const Entity& a, const Entity& b) {
    return std::tuple(a.descriptor.kind, complete_key(a.descriptor)) <
           std::tuple(b.descriptor.kind, complete_key(b.descriptor));
  });
  return result;
}

std::vector<BodyCandidate> candidates(const StepImportGeometry& imported) {
  std::vector<BodyCandidate> result;
  for (std::size_t product = 0; product < imported.products.size(); ++product)
    if (!imported.products[product].is_assembly)
      for (std::size_t body = 0; body < imported.products[product].bodies.size(); ++body)
        result.push_back(candidate(imported.products[product].bodies[body], product, body));
  return result;
}

std::string product_key(const std::vector<BodyCandidate>& bodies, std::size_t product) {
  std::vector<std::string> keys;
  for (const auto& body : bodies)
    if (body.product_index == product)
      keys.push_back(complete_key(body.body.descriptor));
  std::sort(keys.begin(), keys.end());
  std::string result = "product:";
  for (const auto& key : keys)
    result += key;
  return hash(result);
}

bool same(double a, double b) {
  return std::isfinite(a) && std::isfinite(b) &&
         std::abs(a - b) <= kMatchTolerance + 1e-10 * std::max(std::abs(a), std::abs(b));
}

bool same(Point3 a, Point3 b) {
  return same(a.x, b.x) && same(a.y, b.y) && same(a.z, b.z);
}

bool matches(const ImportedTopologyDescriptor& a, const ImportedTopologyDescriptor& b) {
  if (a.kind != b.kind || a.geometry != b.geometry || a.body_kind != b.body_kind ||
      a.geometry_key != b.geometry_key || a.boundary_keys != b.boundary_keys ||
      a.samples.size() != b.samples.size() || !same(a.centroid, b.centroid) ||
      !same(a.origin, b.origin) || !same(a.measure, b.measure) || !same(a.radius_mm, b.radius_mm) ||
      !same(a.direction.x, b.direction.x) || !same(a.direction.y, b.direction.y) ||
      !same(a.direction.z, b.direction.z))
    return false;
  for (std::size_t i = 0; i < a.bounds_mm.size(); ++i)
    if (!same(a.bounds_mm[i], b.bounds_mm[i]))
      return false;
  for (std::size_t i = 0; i < a.samples.size(); ++i)
    if (!same(a.samples[i], b.samples[i]))
      return false;
  return true;
}

std::vector<std::size_t> intersect(const std::vector<std::size_t>& a,
                                   const std::vector<std::size_t>& b) {
  std::vector<std::size_t> result;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
  return result;
}

struct OwnerMatch {
  const ImportedTopologyEntry* entry{};
  std::vector<std::size_t> candidates;
  bool conflict{false};
};

OwnerMatch recover_owner(const ImportedTopologyEntry& body, const ImportedTopologyCatalog& catalog,
                         const std::vector<BodyCandidate>& current,
                         const std::set<std::size_t>& eligible_products) {
  OwnerMatch result{&body, {}, false};
  for (std::size_t i = 0; i < current.size(); ++i)
    if ((eligible_products.empty() || eligible_products.contains(current[i].product_index)) &&
        matches(body.descriptor, current[i].body.descriptor))
      result.candidates.push_back(i);
  if (!result.candidates.empty())
    return result;
  bool anchor_found = false;
  for (const auto& saved : catalog.entries()) {
    if (saved.parent_body != body.id || saved.descriptor.kind != ImportedTopologyKind::Face)
      continue;
    std::vector<std::size_t> owners;
    for (std::size_t i = 0; i < current.size(); ++i) {
      if (!eligible_products.empty() && !eligible_products.contains(current[i].product_index))
        continue;
      if (current[i].body.descriptor.body_kind != body.descriptor.body_kind)
        continue;
      if (std::any_of(
              current[i].entities.begin(), current[i].entities.end(),
              [&](const Entity& entity) { return matches(saved.descriptor, entity.descriptor); }))
        owners.push_back(i);
    }
    if (owners.empty())
      continue;
    if (!anchor_found) {
      result.candidates = owners;
      anchor_found = true;
    } else {
      result.candidates = intersect(result.candidates, owners);
      if (result.candidates.empty())
        result.conflict = true;
    }
  }
  return result;
}

} // namespace

const ImportedTopologyResolution*
ImportedTopologyRecovery::find(ImportedTopologyId id) const noexcept {
  const auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](const auto& entry) { return entry.id == id; });
  return found == entries.end() ? nullptr : &*found;
}

Result<ImportedTopologyCatalog>
StepImportTopology::create_catalog(ImportedTopologyCatalogId id, const StepImportGeometry& imported,
                                   std::size_t product_index) const {
  try {
    require(product_index < imported.products.size() &&
                !imported.products[product_index].is_assembly &&
                !imported.products[product_index].bodies.empty(),
            "import topology catalog requires one nonempty Part product definition");
    auto current = candidates(imported);
    std::vector<std::size_t> ordered;
    for (std::size_t i = 0; i < current.size(); ++i)
      if (current[i].product_index == product_index)
        ordered.push_back(i);
    std::sort(ordered.begin(), ordered.end(), [&](auto a, auto b) {
      return complete_key(current[a].body.descriptor) < complete_key(current[b].body.descriptor);
    });
    std::array<std::size_t, 4> counters{};
    const auto new_id = [&](ImportedTopologyKind kind) {
      return ImportedTopologyId(id.value() + "." + std::string(to_string(kind)) + "." +
                                std::to_string(++counters[static_cast<std::size_t>(kind)]));
    };
    std::vector<ImportedTopologyEntry> entries;
    for (auto index : ordered) {
      const auto body_id = new_id(ImportedTopologyKind::Body);
      entries.push_back({body_id, std::nullopt, current[index].body.descriptor});
      for (const auto& entity : current[index].entities)
        entries.push_back({new_id(entity.descriptor.kind), body_id, entity.descriptor});
    }
    return ImportedTopologyCatalog::create(std::move(id), imported.source.id(),
                                           imported.source.sha256_digest(),
                                           product_key(current, product_index), std::move(entries));
  } catch (const Standard_Failure& failure) {
    return Result<ImportedTopologyCatalog>::failure(invalid(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<ImportedTopologyCatalog>::failure(invalid(exception.what()));
  }
}

Result<ImportedTopologyRecovery>
StepImportTopology::recover(const ImportedTopologyCatalog& catalog,
                            const StepImportGeometry& imported) const {
  try {
    require(catalog.source() == imported.source.id(),
            "import topology catalog source identity does not match current geometry");
    auto current = candidates(imported);
    std::set<std::size_t> exact_products;
    for (std::size_t product = 0; product < imported.products.size(); ++product)
      if (!imported.products[product].is_assembly &&
          product_key(current, product) == catalog.product_key())
        exact_products.insert(product);
    std::vector<OwnerMatch> owners;
    for (const auto& entry : catalog.entries())
      if (entry.descriptor.kind == ImportedTopologyKind::Body)
        owners.push_back(recover_owner(entry, catalog, current, exact_products));
    std::vector<std::size_t> products;
    bool product_evidence = false, product_conflict = false;
    for (const auto& owner : owners) {
      std::vector<std::size_t> available;
      for (auto body : owner.candidates)
        available.push_back(current[body].product_index);
      std::sort(available.begin(), available.end());
      available.erase(std::unique(available.begin(), available.end()), available.end());
      if (available.empty())
        continue;
      if (!product_evidence) {
        products = available;
        product_evidence = true;
      } else {
        products = intersect(products, available);
        product_conflict = products.empty();
      }
    }
    ImportedTopologyRecovery result{
        catalog.id(), catalog.source(), imported.source.sha256_digest(), {}};
    for (const auto& saved : catalog.entries()) {
      ImportedTopologyResolution entry;
      entry.id = saved.id;
      const auto body_id = saved.parent_body ? *saved.parent_body : saved.id;
      auto owner = std::find_if(owners.begin(), owners.end(),
                                [&](const auto& value) { return value.entry->id == body_id; });
      require(owner != owners.end(), "import topology catalog has no enclosing Body entry");
      auto eligible = owner->candidates;
      if (!products.empty())
        std::erase_if(eligible, [&](auto index) {
          return !std::binary_search(products.begin(), products.end(),
                                     current[index].product_index);
        });
      entry.candidate_count = eligible.size();
      if (owner->conflict || product_conflict || eligible.size() > 1) {
        entry.status = ImportedTopologyRecoveryStatus::Ambiguous;
        entry.diagnostic =
            "Imported topology owner has conflicting or indistinguishable complete evidence";
      } else if (eligible.empty()) {
        entry.status = ImportedTopologyRecoveryStatus::Lost;
        entry.diagnostic =
            "Imported topology owner has no complete surviving body or trimmed-face witness";
      } else {
        const auto& body = current[eligible.front()];
        std::vector<const Entity*> found;
        if (saved.descriptor.kind == ImportedTopologyKind::Body)
          found.push_back(&body.body);
        else
          for (const auto& entity : body.entities)
            if (matches(saved.descriptor, entity.descriptor))
              found.push_back(&entity);
        entry.candidate_count = found.size();
        entry.status = found.empty()      ? ImportedTopologyRecoveryStatus::Lost
                       : found.size() > 1 ? ImportedTopologyRecoveryStatus::Ambiguous
                                          : ImportedTopologyRecoveryStatus::Resolved;
        if (found.size() == 1) {
          entry.product_index = body.product_index;
          entry.body_index = body.body_index;
          entry.shape = GeometryShape(std::make_shared<GeometryShape::Impl>(found.front()->shape));
          entry.descriptor = found.front()->descriptor;
          entry.capabilities = imported_topology_capabilities(*entry.descriptor);
          entry.diagnostic = "Imported topology resolved by unique complete normalized evidence";
        } else
          entry.diagnostic =
              found.empty()
                  ? "Imported topology complete witness is lost on the recovered body"
                  : "Imported topology complete witness matches multiple current entities";
      }
      result.entries.push_back(std::move(entry));
    }
    return Result<ImportedTopologyRecovery>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<ImportedTopologyRecovery>::failure(invalid(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<ImportedTopologyRecovery>::failure(invalid(exception.what()));
  }
}

Result<ImportedTopologyResolution>
StepImportTopology::resolve(const ImportedTopologyRecovery& recovery,
                            const ImportedTopologyReference& reference) const {
  if (recovery.catalog != reference.catalog())
    return Result<ImportedTopologyResolution>::failure(
        invalid("import topology reference catalog does not match recovery evidence"));
  const auto* entry = recovery.find(reference.entry());
  if (!entry || entry->status != ImportedTopologyRecoveryStatus::Resolved || !entry->descriptor ||
      entry->shape.empty())
    return Result<ImportedTopologyResolution>::failure(
        invalid(entry ? entry->diagnostic : "import topology reference entry does not exist"));
  if (entry->capabilities != imported_topology_capabilities(*entry->descriptor) ||
      std::find(entry->capabilities.begin(), entry->capabilities.end(), reference.capability()) ==
          entry->capabilities.end())
    return Result<ImportedTopologyResolution>::failure(
        invalid("import topology does not support the requested analytic capability"));
  return Result<ImportedTopologyResolution>::success(*entry);
}

Result<ImportedTopologyResolution>
StepImportTopology::match_subshape(const ImportedTopologyDescriptor& wanted,
                                   const TopoDS_Shape& target) const {
  try {
    require(!target.IsNull(), "import topology downstream target shape is unavailable");
    std::vector<Entity> found;
    const auto kind = wanted.kind == ImportedTopologyKind::Face   ? TopAbs_FACE
                      : wanted.kind == ImportedTopologyKind::Edge ? TopAbs_EDGE
                                                                  : TopAbs_VERTEX;
    require(wanted.kind != ImportedTopologyKind::Body,
            "import topology subshape matching requires Face, Edge or Vertex");
    TopTools_IndexedMapOfShape topology;
    TopExp::MapShapes(target, kind, topology);
    for (int index = 1; index <= topology.Extent(); ++index) {
      auto actual = kind == TopAbs_FACE   ? face_descriptor(TopoDS::Face(topology(index)))
                    : kind == TopAbs_EDGE ? edge_descriptor(TopoDS::Edge(topology(index)))
                                          : vertex_descriptor(TopoDS::Vertex(topology(index)));
      if (matches(wanted, actual))
        found.push_back({std::move(actual), topology(index)});
    }
    ImportedTopologyResolution result;
    result.candidate_count = found.size();
    result.status = found.empty()      ? ImportedTopologyRecoveryStatus::Lost
                    : found.size() > 1 ? ImportedTopologyRecoveryStatus::Ambiguous
                                       : ImportedTopologyRecoveryStatus::Resolved;
    if (found.size() == 1) {
      result.shape = GeometryShape(std::make_shared<GeometryShape::Impl>(found.front().shape));
      result.descriptor = found.front().descriptor;
      result.capabilities = imported_topology_capabilities(*result.descriptor);
      result.diagnostic = "Imported topology uniquely matches the actual downstream target";
    } else
      result.diagnostic = found.empty()
                              ? "Imported topology does not match the actual downstream target"
                              : "Imported topology is ambiguous on the actual downstream target";
    return Result<ImportedTopologyResolution>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<ImportedTopologyResolution>::failure(invalid(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<ImportedTopologyResolution>::failure(invalid(exception.what()));
  }
}

} // namespace blcad::geometry
