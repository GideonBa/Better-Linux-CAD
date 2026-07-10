#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

// Declares that a part parameter follows an assembly parameter. The binding is
// model intent stored in the assembly document; value propagation happens
// through AssemblyDocument::apply_bindings_to.
class ParameterBinding {
public:
  [[nodiscard]] static Result<ParameterBinding> create(ParameterBindingId id,
                                                       DocumentId part_document,
                                                       ParameterId part_parameter,
                                                       ParameterId assembly_parameter);

  [[nodiscard]] const ParameterBindingId& id() const noexcept;
  [[nodiscard]] const DocumentId& part_document() const noexcept;
  [[nodiscard]] const ParameterId& part_parameter() const noexcept;
  [[nodiscard]] const ParameterId& assembly_parameter() const noexcept;

private:
  ParameterBinding(ParameterBindingId id, DocumentId part_document, ParameterId part_parameter,
                   ParameterId assembly_parameter);

  ParameterBindingId id_;
  DocumentId part_document_;
  ParameterId part_parameter_;
  ParameterId assembly_parameter_;
};

// Result of applying assembly bindings to one part document: the bindings that
// changed a part parameter and the affected dependency-graph nodes reported by
// the part.
struct BindingApplication {
  std::size_t applied_binding_count = 0U;
  std::vector<std::string> affected_part_nodes;
};

// MVP-4 seed: an assembly document that owns assembly-scoped parameters,
// registers member parts by DocumentId, and binds part parameters to assembly
// parameters. Component instances, transforms, and constraints are MVP 5.
class AssemblyDocument {
public:
  [[nodiscard]] static Result<AssemblyDocument> create(DocumentId id, std::string name);

  // Only assembly-scoped parameters are allowed.
  [[nodiscard]] Result<std::size_t> add_parameter(Parameter parameter);
  [[nodiscard]] Result<std::size_t> add_member_part(DocumentId part_document);
  // The binding target part must be a registered member and the assembly
  // parameter must exist. The part parameter is validated on application.
  [[nodiscard]] Result<std::size_t> add_binding(ParameterBinding binding);
  // Sets an assembly parameter value with the same validation as creation.
  [[nodiscard]] Result<std::size_t> set_parameter_value(ParameterId id, Quantity value);

  // Pushes bound assembly values into the given part document. Bound part
  // parameters must exist and have the same parameter type as the assembly
  // parameter. Bindings for other parts are skipped. Returns which bindings
  // were applied and the affected part graph nodes.
  [[nodiscard]] Result<BindingApplication> apply_bindings_to(PartDocument& part) const;

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;
  [[nodiscard]] const std::vector<DocumentId>& member_parts() const noexcept;
  [[nodiscard]] const std::vector<ParameterBinding>& bindings() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::size_t member_part_count() const noexcept;
  [[nodiscard]] std::size_t binding_count() const noexcept;
  [[nodiscard]] const Parameter* find_parameter(ParameterId id) const noexcept;
  [[nodiscard]] const ParameterBinding* find_binding(ParameterBindingId id) const noexcept;
  [[nodiscard]] bool has_member_part(const DocumentId& part_document) const noexcept;

private:
  AssemblyDocument(DocumentId id, std::string name);

  [[nodiscard]] bool has_parameter_id(const ParameterId& id) const noexcept;
  [[nodiscard]] bool has_parameter_name(std::string_view name) const noexcept;
  [[nodiscard]] bool has_binding_id(const ParameterBindingId& id) const noexcept;

  DocumentId id_;
  std::string name_;
  std::vector<Parameter> parameters_;
  std::vector<DocumentId> member_parts_;
  std::vector<ParameterBinding> bindings_;
};

} // namespace blcad
