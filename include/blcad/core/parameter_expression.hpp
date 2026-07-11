#pragma once

#include "blcad/core/parameter.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class PartDocument;

struct ParameterExpressionEvaluation {
  Quantity value;
  // Distinct referenced parameter ids in first-use order.
  std::vector<ParameterId> referenced_parameters;
};

// Deterministic unit-aware evaluation of a parameter formula against the
// parameters of one part document. Grammar:
//
//   expression := term (('+' | '-') term)*
//   term       := factor (('*' | '/') factor)*
//   factor     := number ['mm'] | parameter-name | '(' expression ')' | '-' factor
//
// Parameter references use the parameter *name*. Unit dimension is tracked as
// a length power: bare numbers and Count parameters are dimensionless, `mm`
// literals and Length parameters have power one. Addition and subtraction
// require equal powers; multiplication adds and division subtracts powers,
// and any intermediate or final power outside {0, 1} is rejected. The final
// power must match the target parameter type (Length -> 1, Count -> 0), and
// the result must satisfy that type's value validation.
class ParameterExpressionEvaluator {
public:
  [[nodiscard]] Result<ParameterExpressionEvaluation> evaluate(const PartDocument& document,
                                                               std::string_view formula,
                                                               ParameterType target_type,
                                                               std::string_view object_id) const;
};

} // namespace blcad
