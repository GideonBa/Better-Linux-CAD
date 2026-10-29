#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/occt_shape_view.hpp"

#include "blcad/core/step_import_freshness.hpp"

#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomTools.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Check.hxx>
#include <Interface_CheckIterator.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPConstruct_ExternRefs.hxx>
#include <STEPConstruct_UnitContext.hxx>
#include <STEPControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <StepBasic_ConversionBasedUnitAndLengthUnit.hxx>
#include <StepBasic_LengthUnit.hxx>
#include <StepBasic_MeasureWithUnit.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_SiUnitAndLengthUnit.hxx>
#include <StepBasic_Unit.hxx>
#include <StepData_GlobalFactors.hxx>
#include <StepData_StepModel.hxx>
#include <StepGeom_CartesianPoint.hxx>
#include <StepGeom_CartesianTransformationOperator.hxx>
#include <StepGeom_Direction.hxx>
#include <StepGeom_GeomRepContextAndGlobUnitAssCtxAndGlobUncertaintyAssCtx.hxx>
#include <StepGeom_GeometricRepresentationContextAndGlobalUnitAssignedContext.hxx>
#include <StepRepr_AssemblyComponentUsage.hxx>
#include <StepRepr_GlobalUnitAssignedContext.hxx>
#include <StepShape_ShapeRepresentation.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <Transfer_TransientProcess.hxx>
#include <UnitsMethods.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XSAlgo.hxx>
#include <XSAlgo_AlgoContainer.hxx>
#include <XSControl_WorkSession.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kReaderId = "geometry.step_import_reader";

Error invalid(std::string message) {
  return Error::geometry(kReaderId, std::move(message));
}

void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}

// OCCT's optional FromSTEP ShapeFix sequence is otherwise selected through mutable resource
// files. This adapter accepts valid transferred topology and never silently repairs it.
class UnhealedTransfer final : public XSAlgo_AlgoContainer {
public:
  TopoDS_Shape ProcessShape(const TopoDS_Shape& shape, Standard_Real, Standard_Real,
                            Standard_CString, Standard_CString, Handle(Standard_Transient) & info,
                            const Message_ProgressRange&, Standard_Boolean) const override {
    info.Nullify();
    return shape;
  }
};

class ReaderSettings {
public:
  ReaderSettings()
      : previous_algorithm_(XSAlgo::AlgoContainer()),
        previous_naming_(XCAFDoc_ShapeTool::AutoNaming()),
        previous_unit_(UnitsMethods::GetCasCadeLengthUnit()),
        previous_step_unit_(StepData_GlobalFactors::Intance().CascadeUnit()),
        previous_factors_{StepData_GlobalFactors::Intance().LengthFactor(),
                          StepData_GlobalFactors::Intance().PlaneAngleFactor(),
                          StepData_GlobalFactors::Intance().SolidAngleFactor()} {
    try {
      set("xstep.cascade.unit", "MM");
      set("read.step.product.mode", "ON");
      set("read.step.product.context", "all");
      set("read.step.assembly.level", "all");
      set("read.step.shape.repr", "All");
      set("read.step.shape.relationship", "ON");
      set("read.step.shape.aspect", "ON");
      set("read.step.nonmanifold", "Off");
      set("read.step.ideas", "Off");
      set("step.angleunit.mode", "File");
      XSAlgo::SetAlgoContainer(new UnhealedTransfer);
      XCAFDoc_ShapeTool::SetAutoNaming(false);
    } catch (...) {
      restore();
      throw;
    }
  }
  ~ReaderSettings() {
    restore();
  }

private:
  void restore() {
    XSAlgo::SetAlgoContainer(previous_algorithm_);
    XCAFDoc_ShapeTool::SetAutoNaming(previous_naming_);
    UnitsMethods::SetCasCadeLengthUnit(previous_unit_);
    auto& factors = StepData_GlobalFactors::Intance();
    factors.SetCascadeUnit(previous_step_unit_);
    factors.InitializeFactors(previous_factors_[0], previous_factors_[1], previous_factors_[2]);
    for (auto it = settings_.rbegin(); it != settings_.rend(); ++it)
      Interface_Static::SetCVal(it->first.c_str(), it->second.c_str());
  }
  void set(const char* key, const char* value) {
    const char* prior = Interface_Static::CVal(key);
    require(prior != nullptr, "required OCCT STEP reader setting is unavailable");
    settings_.emplace_back(key, prior);
    require(Interface_Static::SetCVal(key, value), "could not freeze OCCT STEP reader setting");
  }
  Handle(XSAlgo_AlgoContainer) previous_algorithm_;
  bool previous_naming_;
  double previous_unit_;
  double previous_step_unit_;
  std::array<double, 3> previous_factors_;
  std::vector<std::pair<std::string, std::string>> settings_;
};

std::mutex reader_mutex;

std::string digest(std::string_view data) {
  auto result = step_import_sha256(data);
  if (!result)
    throw std::runtime_error(result.error().message());
  return result.value();
}

void number(std::ostream& stream, double value) {
  require(std::isfinite(value), "STEP geometry contains a non-finite numeric value");
  stream << (value == 0 ? 0.0 : value) << ',';
}

void point(std::ostream& stream, const gp_Pnt& value) {
  number(stream, value.X());
  number(stream, value.Y());
  number(stream, value.Z());
}

StepImportTransform transform(const gp_Trsf& value) {
  StepImportTransform result;
  for (int row = 1; row <= 3; ++row)
    for (int column = 1; column <= 4; ++column) {
      const double entry = value.Value(row, column);
      require(std::isfinite(entry), "STEP occurrence transform contains non-finite values");
      result.matrix[static_cast<std::size_t>((row - 1) * 4 + column - 1)] = entry == 0 ? 0 : entry;
    }
  const auto& m = result.matrix;
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t other = 0; other < 3; ++other) {
      double dot = 0;
      for (std::size_t column = 0; column < 3; ++column)
        dot += m[row * 4 + column] * m[other * 4 + column];
      require(std::abs(dot - (row == other ? 1.0 : 0.0)) < 1e-8,
              "STEP occurrence transform must be rigid without scaling");
    }
  const double det = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) +
                     m[2] * (m[4] * m[9] - m[5] * m[8]);
  require(std::abs(det - 1.0) < 1e-8,
          "STEP occurrence transform must preserve orientation without reflection");
  return result;
}

gp_Trsf occt_transform(const StepImportTransform& value) {
  for (double entry : value.matrix)
    require(std::isfinite(entry), "STEP preview transform contains non-finite values");
  // Validate before SetValues, which orthogonalizes its input and could hide invalid matrices.
  const auto& m = value.matrix;
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t other = 0; other < 3; ++other) {
      double dot = 0;
      for (std::size_t column = 0; column < 3; ++column)
        dot += m[row * 4 + column] * m[other * 4 + column];
      require(std::abs(dot - (row == other ? 1.0 : 0.0)) < 1e-8,
              "STEP preview transform must be rigid");
    }
  gp_Trsf result;
  result.SetValues(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11]);
  (void)transform(result);
  return result;
}

void write_transform(std::ostream& stream, const gp_Trsf& value) {
  for (double entry : transform(value).matrix)
    number(stream, entry);
}

// Recursive unordered child multisets eliminate OCCT exploration order. Curve/surface equations,
// local placements, ranges, orientations and topology incidence remain in the descriptor.
// This is derived ordering evidence, deliberately not a persisted topology identity.
std::string shape_descriptor(const TopoDS_Shape& shape) {
  require(!shape.IsNull(), "STEP body contains null topology");
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(std::numeric_limits<double>::max_digits10)
         << static_cast<int>(shape.ShapeType()) << ':' << static_cast<int>(shape.Orientation())
         << ':';
  write_transform(stream, shape.Location().Transformation());
  if (shape.ShapeType() == TopAbs_VERTEX) {
    point(stream, BRep_Tool::Pnt(TopoDS::Vertex(shape)));
  } else if (shape.ShapeType() == TopAbs_EDGE) {
    TopLoc_Location location;
    double first = 0, last = 0;
    auto curve = BRep_Tool::Curve(TopoDS::Edge(shape), location, first, last);
    if (!curve.IsNull()) {
      GeomTools::Write(curve, stream);
      write_transform(stream, location.Transformation());
      number(stream, first);
      number(stream, last);
    } else {
      require(BRep_Tool::Degenerated(TopoDS::Edge(shape)), "STEP edge has no geometric curve");
      stream << "degenerated";
    }
  } else if (shape.ShapeType() == TopAbs_FACE) {
    TopLoc_Location location;
    auto surface = BRep_Tool::Surface(TopoDS::Face(shape), location);
    require(!surface.IsNull(), "STEP face has no geometric surface");
    GeomTools::Write(surface, stream);
    write_transform(stream, location.Transformation());
  }
  std::vector<std::string> children;
  for (TopoDS_Iterator it(shape, false, false); it.More(); it.Next())
    children.push_back(shape_descriptor(it.Value()));
  std::sort(children.begin(), children.end());
  for (const auto& child : children)
    stream << child.size() << ':' << child;
  return digest(stream.str());
}

std::string name_of(const TDF_Label& label) {
  Handle(TDataStd_Name) name;
  if (!label.FindAttribute(TDataStd_Name::GetID(), name))
    return {};
  const auto& extended = name->Get();
  std::string result(static_cast<std::size_t>(extended.LengthOfCString()) + 1, '\0');
  char* target = result.data();
  const auto count = extended.ToUTF8CString(target);
  result.resize(static_cast<std::size_t>(count));
  return result;
}

struct Unit {
  std::string name;
  double scale;
};

Unit length_unit(const Handle(StepBasic_NamedUnit) & unit, std::size_t depth = 0) {
  require(!unit.IsNull() && depth < 16, "STEP length unit is absent or recursively ambiguous");
  auto si = Handle(StepBasic_SiUnitAndLengthUnit)::DownCast(unit);
  if (!si.IsNull()) {
    require(si->Name() == StepBasic_sunMetre, "unsupported STEP SI length unit");
    const double factor =
        si->HasPrefix() ? STEPConstruct_UnitContext::ConvertSiPrefix(si->Prefix()) : 1;
    const double scale = factor * 1000;
    require(std::isfinite(scale) && scale > 0, "invalid STEP SI length scale");
    std::string name = scale == 1 ? "millimetre" : scale == 1000 ? "metre" : "SI metre prefix";
    return {std::move(name), scale};
  }
  auto conversion = Handle(StepBasic_ConversionBasedUnitAndLengthUnit)::DownCast(unit);
  if (!conversion.IsNull()) {
    auto factor = conversion->ConversionFactor();
    require(!factor.IsNull(), "STEP conversion-based length unit has no conversion factor");
    auto base = length_unit(factor->UnitComponent().NamedUnit(), depth + 1);
    const double scale = factor->ValueComponent() * base.scale;
    require(std::isfinite(scale) && scale > 0, "invalid STEP conversion-based length scale");
    return {conversion->Name().IsNull() ? "conversion-based length"
                                        : conversion->Name()->ToCString(),
            scale};
  }
  throw std::runtime_error("unsupported or ambiguous STEP length unit");
}

Handle(StepRepr_GlobalUnitAssignedContext)
    unit_context(const Handle(StepRepr_RepresentationContext) & context) {
  auto direct = Handle(StepRepr_GlobalUnitAssignedContext)::DownCast(context);
  if (!direct.IsNull())
    return direct;
  auto simple = Handle(
      StepGeom_GeometricRepresentationContextAndGlobalUnitAssignedContext)::DownCast(context);
  if (!simple.IsNull())
    return simple->GlobalUnitAssignedContext();
  auto combined =
      Handle(StepGeom_GeomRepContextAndGlobUnitAssCtxAndGlobUncertaintyAssCtx)::DownCast(context);
  return combined.IsNull() ? Handle(StepRepr_GlobalUnitAssignedContext){}
                           : combined->GlobalUnitAssignedContext();
}

std::vector<StepImportUnitDiagnostic> inspect_model(const Handle(StepData_StepModel) & model) {
  require(!model.IsNull(), "STEP parser produced no model");
  std::vector<StepImportUnitDiagnostic> units;
  for (int index = 0; index <= model->NbEntities(); ++index) {
    auto check = index == 0 ? model->GlobalCheck() : model->Check(index, true);
    require(check.IsNull() || !check->HasFailed(),
            "STEP contains syntax or entity validation errors");
    if (index == 0)
      continue;
    const auto& entity = model->Value(index);
    auto point = Handle(StepGeom_CartesianPoint)::DownCast(entity);
    if (!point.IsNull()) {
      require(point->NbCoordinates() == 2 || point->NbCoordinates() == 3,
              "STEP Cartesian point has unsupported dimension");
      for (int coordinate = 1; coordinate <= point->NbCoordinates(); ++coordinate)
        require(std::isfinite(point->CoordinatesValue(coordinate)),
                "STEP point or transform contains non-finite coordinates");
    }
    auto direction = Handle(StepGeom_Direction)::DownCast(entity);
    if (!direction.IsNull()) {
      double magnitude = 0;
      for (int coordinate = 1; coordinate <= direction->NbDirectionRatios(); ++coordinate) {
        const double value = direction->DirectionRatiosValue(coordinate);
        require(std::isfinite(value), "STEP transform direction contains non-finite values");
        magnitude += value * value;
      }
      require(std::isfinite(magnitude) && magnitude > 0,
              "STEP transform direction must be nonzero and finite");
    }
    auto operation = Handle(StepGeom_CartesianTransformationOperator)::DownCast(entity);
    if (!operation.IsNull() && operation->HasScale())
      require(std::isfinite(operation->Scale()) && std::abs(operation->Scale() - 1) < 1e-12,
              "STEP occurrence scaling is unsupported");
    auto representation = Handle(StepShape_ShapeRepresentation)::DownCast(entity);
    if (representation.IsNull())
      continue;
    auto context = unit_context(representation->ContextOfItems());
    require(!context.IsNull(), "STEP shape representation has no declared length-unit context");
    STEPConstruct_UnitContext decoder;
    require(decoder.ComputeFactors(context) == 0 && decoder.LengthDone(),
            "STEP shape representation has unsupported units");
    std::optional<Unit> length;
    for (int u = 1; u <= context->NbUnits(); ++u) {
      const auto candidate = context->UnitsValue(u);
      require(!candidate.IsNull(), "STEP unit context contains a missing unit");
      if (candidate->IsKind(STANDARD_TYPE(StepBasic_SiUnitAndLengthUnit)) ||
          candidate->IsKind(STANDARD_TYPE(StepBasic_ConversionBasedUnitAndLengthUnit)) ||
          candidate->IsKind(STANDARD_TYPE(StepBasic_LengthUnit))) {
        require(!length.has_value(), "STEP shape representation has multiple length units");
        length = length_unit(candidate);
      }
    }
    require(length.has_value(), "STEP shape representation has no supported declared length unit");
    units.push_back({length->name, length->scale});
  }
  require(!units.empty(), "STEP contains no supported geometric representation or declared units");
  std::sort(units.begin(), units.end(), [](const auto& a, const auto& b) {
    return std::tie(a.length_to_mm, a.declared_length_unit) <
           std::tie(b.length_to_mm, b.declared_length_unit);
  });
  units.erase(std::unique(units.begin(), units.end(),
                          [](const auto& a, const auto& b) {
                            return a.length_to_mm == b.length_to_mm &&
                                   a.declared_length_unit == b.declared_length_unit;
                          }),
              units.end());
  return units;
}

struct BodyDraft {
  StepImportedBodyDefinition definition;
  TopoDS_Shape shape;
};

BodyDraft body_draft(const TopoDS_Shape& shape, std::string name) {
  require(!shape.IsNull() && BRepCheck_Analyzer(shape, true).IsValid(),
          "STEP transfer produced invalid body geometry; automatic healing is disabled");
  BodyDraft result;
  result.shape = shape;
  auto& value = result.definition;
  value.name = std::move(name);
  value.kind = shape.ShapeType() == TopAbs_SOLID ? BodyKind::Solid : BodyKind::Surface;
  value.source_kind = shape.ShapeType() == TopAbs_SOLID   ? StepImportedShapeKind::Solid
                      : shape.ShapeType() == TopAbs_SHELL ? StepImportedShapeKind::Shell
                                                          : StepImportedShapeKind::Face;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
    ++value.face_count;
  require(value.face_count > 0, "STEP body contains no face topology");
  Bnd_Box box;
  BRepBndLib::AddOptimal(shape, box, false, false);
  require(!box.IsVoid() && !box.IsOpen(), "STEP body has empty or unbounded geometry");
  box.Get(value.bounds_mm[0], value.bounds_mm[1], value.bounds_mm[2], value.bounds_mm[3],
          value.bounds_mm[4], value.bounds_mm[5]);
  for (double bound : value.bounds_mm)
    require(std::isfinite(bound), "STEP body has non-finite bounds");
  GProp_GProps area;
  BRepGProp::SurfaceProperties(shape, area);
  value.area_mm2 = area.Mass();
  require(std::isfinite(value.area_mm2) && value.area_mm2 > 0,
          "STEP body must have finite positive surface area");
  if (value.kind == BodyKind::Solid) {
    GProp_GProps volume;
    BRepGProp::VolumeProperties(shape, volume);
    value.volume_mm3 = volume.Mass();
    require(std::isfinite(value.volume_mm3) && value.volume_mm3 > 0,
            "STEP solid must have finite positive volume");
  }
  value.definition_key = shape_descriptor(shape);
  return result;
}

struct DraftProduct {
  TDF_Label label;
  StepImportedProductDefinition definition;
  std::vector<BodyDraft> bodies;
  bool active{true};
};

struct DraftGraph {
  Handle(XCAFDoc_ShapeTool) tool;
  std::vector<DraftProduct> products;
  std::vector<StepImportedOccurrence> occurrences;
  std::vector<std::string> diagnostics;

  void collect_bodies(std::size_t product, const TopoDS_Shape& shape) {
    if (shape.ShapeType() == TopAbs_SOLID || shape.ShapeType() == TopAbs_SHELL ||
        shape.ShapeType() == TopAbs_FACE) {
      TDF_Label subshape;
      const std::string name = tool->FindSubShape(products[product].label, shape, subshape)
                                   ? name_of(subshape)
                                   : products[product].definition.name;
      products[product].bodies.push_back(body_draft(shape, name));
      return;
    }
    require(shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID,
            "STEP product contains unsupported standalone wire, edge or vertex geometry");
    diagnostics.push_back(
        "Compound product geometry is decomposed into local solid/surface bodies");
    for (TopoDS_Iterator it(shape); it.More(); it.Next())
      collect_bodies(product, it.Value());
  }

  std::size_t add_definition(const TDF_Label& label) {
    for (std::size_t i = 0; i < products.size(); ++i)
      if (products[i].label == label) {
        require(!products[i].active, "STEP product hierarchy contains a definition cycle");
        return i;
      }
    const std::size_t index = products.size();
    DraftProduct draft;
    draft.label = label;
    draft.definition.name = name_of(label);
    draft.definition.is_assembly = XCAFDoc_ShapeTool::IsAssembly(label);
    products.push_back(std::move(draft));
    if (products[index].definition.is_assembly) {
      TDF_LabelSequence children;
      XCAFDoc_ShapeTool::GetComponents(label, children, false);
      require(children.Length() > 0, "STEP assembly definition is empty");
      for (int i = 1; i <= children.Length(); ++i)
        add_occurrence(children.Value(i), index);
    } else {
      TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
      require(!shape.IsNull(), "STEP product has no transferred geometry");
      products[index].definition.source_kind =
          shape.ShapeType() == TopAbs_SOLID   ? StepImportedShapeKind::Solid
          : shape.ShapeType() == TopAbs_SHELL ? StepImportedShapeKind::Shell
          : shape.ShapeType() == TopAbs_FACE  ? StepImportedShapeKind::Face
                                              : StepImportedShapeKind::Compound;
      shape.Location(TopLoc_Location{});
      collect_bodies(index, shape);
      require(!products[index].bodies.empty(), "STEP product has no solid or surface body");
      auto& bodies = products[index].bodies;
      std::sort(bodies.begin(), bodies.end(), [](const auto& a, const auto& b) {
        return std::tie(a.definition.definition_key, a.definition.name) <
               std::tie(b.definition.definition_key, b.definition.name);
      });
      for (std::size_t i = 1; i < bodies.size(); ++i)
        if (bodies[i - 1].definition.definition_key == bodies[i].definition.definition_key) {
          bodies[i - 1].definition.ambiguous_order = bodies[i].definition.ambiguous_order = true;
          diagnostics.push_back(
              "Indistinguishable body definitions retain multiplicity; ordering is ambiguous");
        }
    }
    std::vector<std::string> children;
    for (const auto& occurrence : occurrences)
      if (occurrence.parent_definition == index) {
        std::ostringstream signature;
        signature.imbue(std::locale::classic());
        signature << std::setprecision(std::numeric_limits<double>::max_digits10)
                  << products[occurrence.product_definition].definition.definition_key;
        for (double entry : occurrence.transform.matrix)
          number(signature, entry);
        children.push_back(signature.str());
      }
    for (const auto& body : products[index].bodies)
      children.push_back(body.definition.definition_key);
    std::sort(children.begin(), children.end());
    std::string signature = products[index].definition.is_assembly ? "assembly" : "part";
    for (const auto& child : children)
      signature += std::to_string(child.size()) + ':' + child;
    products[index].definition.definition_key = digest(signature);
    products[index].active = false;
    return index;
  }

  void add_occurrence(TDF_Label label, std::optional<std::size_t> parent) {
    const std::string name = name_of(label);
    gp_Trsf location;
    std::vector<TDF_Label> references;
    while (XCAFDoc_ShapeTool::IsReference(label)) {
      require(std::find(references.begin(), references.end(), label) == references.end(),
              "STEP XDE reference hierarchy contains a cycle");
      references.push_back(label);
      location.Multiply(XCAFDoc_ShapeTool::GetLocation(label).Transformation());
      TDF_Label referred;
      require(XCAFDoc_ShapeTool::GetReferredShape(label, referred) && !referred.IsNull(),
              "STEP occurrence has no referred product definition");
      label = referred;
    }
    location.Multiply(XCAFDoc_ShapeTool::GetLocation(label).Transformation());
    const auto placement = transform(location);
    const auto definition = add_definition(label);
    occurrences.push_back({parent, definition, name, placement});
  }
};

// Reject malformed foreign usage graphs before CAF transfer can recursively
// expand them. Pointer identity here belongs solely to this immutable STEP model.
void inspect_assembly_usage_graph(const Handle(StepData_StepModel) & model) {
  using Product = const StepBasic_ProductDefinition*;
  std::map<Product, std::vector<Product>> children;
  std::size_t usages = 0;
  for (int index = 1; index <= model->NbEntities(); ++index) {
    const auto usage = Handle(StepRepr_AssemblyComponentUsage)::DownCast(model->Value(index));
    if (usage.IsNull())
      continue;
    const auto parent = usage->RelatingProductDefinition();
    const auto child = usage->RelatedProductDefinition();
    require(!parent.IsNull() && !child.IsNull() && model->Number(parent) > 0 &&
                model->Number(child) > 0,
            "STEP assembly usage refers to a missing product definition");
    require(parent != child, "STEP assembly usage graph contains a self cycle");
    require(++usages <= 100000, "STEP assembly usage graph exceeds the import limit");
    children[parent.get()].push_back(child.get());
    children.try_emplace(child.get());
  }
  std::map<Product, unsigned> state;
  std::map<Product, std::size_t> depth;
  struct Frame {
    Product product;
    std::size_t next{};
  };
  for (const auto& [product, descendants] : children) {
    (void)descendants;
    if (state[product] == 2)
      continue;
    std::vector<Frame> pending{{product, 0}};
    state[product] = 1;
    while (!pending.empty()) {
      auto& frame = pending.back();
      const auto& next = children.at(frame.product);
      if (frame.next == next.size()) {
        std::size_t longest = 1;
        for (const auto child : next)
          longest = std::max(longest, depth.at(child) + 1);
        require(longest <= 512, "STEP assembly usage depth exceeds the import limit");
        depth[frame.product] = longest;
        state[frame.product] = 2;
        pending.pop_back();
        continue;
      }
      const auto child = next[frame.next++];
      require(state[child] != 1, "STEP assembly usage graph contains a product cycle");
      if (state[child] == 0) {
        require(pending.size() < 512, "STEP assembly usage depth exceeds the import limit");
        state[child] = 1;
        pending.push_back({child, 0});
      }
    }
  }
}

// Share rooted placement validation between headless reading and disposable presentation. No
// tessellation, geometry copying, healing or body descriptor recomputation is needed for this walk.
template <typename Visitor>
void visit_placed_bodies(const StepImportGeometry& imported, Visitor&& visitor) {
  std::vector<gp_Trsf> direct;
  std::vector<std::vector<std::size_t>> children(imported.products.size());
  std::vector<std::size_t> roots;
  direct.reserve(imported.occurrences.size());
  for (std::size_t index = 0; index < imported.occurrences.size(); ++index) {
    const auto& occurrence = imported.occurrences[index];
    require(occurrence.product_definition < imported.products.size() &&
                (!occurrence.parent_definition ||
                 *occurrence.parent_definition < imported.products.size()),
            "STEP occurrence refers to an unavailable definition");
    direct.push_back(occt_transform(occurrence.transform));
    if (occurrence.parent_definition)
      children[*occurrence.parent_definition].push_back(index);
    else
      roots.push_back(index);
  }
  std::vector<bool> active(imported.products.size(), false);
  std::size_t body_count = 0;
  const auto visit = [&](const auto& self, std::size_t index, const gp_Trsf& placement) -> void {
    // Finite direct transforms can still overflow when multiplied along an occurrence chain.
    (void)transform(placement);
    require(!active[index], "STEP product hierarchy contains a cycle");
    active[index] = true;
    for (const auto& body : imported.products[index].bodies) {
      const auto* local = OcctShapeView::get(body.shape);
      require(local != nullptr && !local->IsNull() && body_count < 100000,
              "STEP body is unavailable or occurrence expansion exceeds the limit");
      TopoDS_Shape placed = local->Moved(TopLoc_Location(placement));
      // A valid world placement can also overflow against a finite body-local Location.
      (void)transform(placed.Location().Transformation());
      ++body_count;
      visitor(placed);
    }
    for (const auto child_index : children[index]) {
      gp_Trsf composed = placement;
      composed.Multiply(direct[child_index]);
      self(self, imported.occurrences[child_index].product_definition, composed);
    }
    active[index] = false;
  };
  for (const auto root_index : roots)
    visit(visit, imported.occurrences[root_index].product_definition, direct[root_index]);
  require(body_count > 0, "STEP contains no rooted body occurrences");
}

} // namespace

std::size_t StepImportGeometry::body_count() const noexcept {
  std::size_t count = 0;
  for (const auto& product : products)
    count += product.bodies.size();
  return count;
}

Result<StepImportGeometry>
StepImportReader::read(const StepImportSource& source,
                       const std::filesystem::path& root_directory) const {
  auto bytes = read_verified_step_import_source(source, root_directory);
  if (!bytes)
    return Result<StepImportGeometry>::failure(bytes.error());
  const std::lock_guard lock(reader_mutex);
  try {
    STEPCAFControl_Reader reader;
    ReaderSettings settings;
    reader.SetNameMode(true);
    reader.SetColorMode(false);
    reader.SetLayerMode(false);
    reader.SetPropsMode(false);
    reader.SetSHUOMode(false);
    reader.SetGDTMode(false);
    reader.SetMatMode(false);
    reader.SetViewMode(false);
    std::istringstream stream(bytes.value());
    require(reader.ChangeReader().ReadStream("verified-step-asset.step", stream) ==
                IFSelect_RetDone,
            "could not parse STEP source");
    STEPConstruct_ExternRefs external(reader.ChangeReader().WS());
    external.LoadExternRefs();
    require(external.NbExternRefs() == 0,
            "external STEP file references are unsupported; register a self-contained STEP asset");
    StepImportGeometry result{source, {}, {}, {}, {}};
    result.units = inspect_model(reader.ChangeReader().StepModel());
    inspect_assembly_usage_graph(reader.ChangeReader().StepModel());
    Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(document->Main());
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1.0, UnitsMethods_LengthUnit_Millimeter);
    reader.ChangeReader().SetSystemLengthUnit(1.0);
    require(reader.Transfer(document), "STEP source transferred no supported geometry");
    require(reader.ExternFiles().IsEmpty(), "STEP transfer unexpectedly resolved an external file");
    auto checks = reader.ChangeReader().WS()->MapReader()->CheckList(false);
    for (checks.Start(); checks.More(); checks.Next()) {
      const auto check = checks.Value();
      require(check.IsNull() || !check->HasFailed(), "STEP geometry transfer reported a failure");
      if (!check.IsNull())
        for (int warning = 1; warning <= check->NbWarnings(); ++warning)
          result.diagnostics.push_back(std::string("OCCT transfer warning: ") +
                                       check->CWarning(warning));
    }
    DraftGraph graph;
    graph.tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    require(!graph.tool.IsNull(), "STEP transfer produced no XDE shape tool");
    TDF_LabelSequence roots;
    graph.tool->GetFreeShapes(roots);
    require(roots.Length() > 0, "STEP transfer produced no product roots");
    for (int i = 1; i <= roots.Length(); ++i)
      graph.add_occurrence(roots.Value(i), std::nullopt);
    std::vector<std::size_t> order(graph.products.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      const auto& left = graph.products[a].definition;
      const auto& right = graph.products[b].definition;
      return std::tie(left.definition_key, left.name) < std::tie(right.definition_key, right.name);
    });
    std::vector<std::size_t> remap(order.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
      remap[order[index]] = index;
      auto& draft = graph.products[order[index]];
      for (auto& body : draft.bodies) {
        body.definition.shape = GeometryShape(std::make_shared<GeometryShape::Impl>(body.shape));
        draft.definition.bodies.push_back(std::move(body.definition));
      }
      result.products.push_back(std::move(draft.definition));
    }
    for (std::size_t i = 1; i < result.products.size(); ++i)
      if (result.products[i - 1].definition_key == result.products[i].definition_key) {
        result.products[i - 1].ambiguous_order = result.products[i].ambiguous_order = true;
        graph.diagnostics.push_back("Indistinguishable product definitions retain separate "
                                    "definitions; ordering is ambiguous");
      }
    for (auto& occurrence : graph.occurrences) {
      occurrence.product_definition = remap[occurrence.product_definition];
      if (occurrence.parent_definition)
        occurrence.parent_definition = remap[*occurrence.parent_definition];
      result.occurrences.push_back(std::move(occurrence));
    }
    std::sort(
        result.occurrences.begin(), result.occurrences.end(), [](const auto& a, const auto& b) {
          return std::tie(a.parent_definition, a.product_definition, a.transform.matrix, a.name) <
                 std::tie(b.parent_definition, b.product_definition, b.transform.matrix, b.name);
        });
    std::sort(graph.diagnostics.begin(), graph.diagnostics.end());
    graph.diagnostics.erase(std::unique(graph.diagnostics.begin(), graph.diagnostics.end()),
                            graph.diagnostics.end());
    result.diagnostics.insert(result.diagnostics.end(), graph.diagnostics.begin(),
                              graph.diagnostics.end());
    result.diagnostics.push_back(
        "Declared source units normalized to millimetres at the Geometry boundary");
    result.diagnostics.push_back("OCCT optional ShapeFix/healing and adapter sewing are disabled; "
                                 "invalid transfers are rejected");
    require(result.body_count() > 0, "STEP transfer produced no supported body definitions");
    visit_placed_bodies(result, [](const TopoDS_Shape&) {});
    return Result<StepImportGeometry>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<StepImportGeometry>::failure(invalid(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<StepImportGeometry>::failure(invalid(exception.what()));
  } catch (...) {
    return Result<StepImportGeometry>::failure(invalid("unknown STEP import failure"));
  }
}

Result<std::vector<GeometryShape>>
StepImportReader::preview_shapes(const StepImportGeometry& imported) const {
  try {
    std::vector<GeometryShape> shapes;
    visit_placed_bodies(imported, [&](const TopoDS_Shape& placed) {
      shapes.push_back(GeometryShape(std::make_shared<GeometryShape::Impl>(placed)));
    });
    return Result<std::vector<GeometryShape>>::success(std::move(shapes));
  } catch (const Standard_Failure& failure) {
    return Result<std::vector<GeometryShape>>::failure(invalid(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<std::vector<GeometryShape>>::failure(invalid(exception.what()));
  }
}

} // namespace blcad::geometry
