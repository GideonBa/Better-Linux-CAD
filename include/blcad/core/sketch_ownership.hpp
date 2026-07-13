#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <string_view>

namespace blcad {

enum class SketchAssociation { DrivesBody, ConsumedByBody, ReferenceOnly };

[[nodiscard]] std::string_view to_string(SketchAssociation association) noexcept;

class SketchOwnership {
public:
  [[nodiscard]] static Result<SketchOwnership> create(SketchId sketch, BodyId owning_body,
                                                      SketchAssociation association);

  [[nodiscard]] const SketchId& sketch() const noexcept;
  [[nodiscard]] const BodyId& owning_body() const noexcept;
  [[nodiscard]] SketchAssociation association() const noexcept;

  friend bool operator==(const SketchOwnership&, const SketchOwnership&) = default;

private:
  SketchOwnership(SketchId sketch, BodyId owning_body, SketchAssociation association);

  SketchId sketch_;
  BodyId owning_body_;
  SketchAssociation association_ = SketchAssociation::ReferenceOnly;
};

} // namespace blcad
