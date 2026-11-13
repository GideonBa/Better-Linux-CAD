#pragma once
#include "blcad/core/curve_expression.hpp"
#include "blcad/core/parameter.hpp"
#include <cstddef>

namespace blcad {
namespace curve_expression_detail {
enum class CurveOp {
  Constant,
  Variable,
  Parameter,
  Add,
  Subtract,
  Multiply,
  Divide,
  Negate,
  Power,
  Sin,
  Cos,
  Tan,
  Sqrt,
  Exp,
  Log,
  Abs
};
struct CurveExpressionNode {
  CurveOp op{CurveOp::Constant};
  std::size_t a{}, b{};
  double number{};
  int exponent{};
  std::string parameter;
  CurveExpressionUnit unit;
  bool unknown_unit{false};
  bool variable{false};
  bool pi_symbol{false};
  double pi_scale{1};
  int pi_divisor{1};
};
struct CurveExpressionData {
  std::vector<CurveExpressionNode> nodes;
  std::vector<ParameterId> dependencies;
  std::optional<DocumentId> document;
  std::optional<CurveExpressionUnit> variable_unit;
  std::string object_id;
};
} // namespace curve_expression_detail
namespace curve_expression_detail {
inline CurveExpressionUnit parameter_unit(const Parameter& parameter) {
  if (parameter.type() == ParameterType::Length)
    return {1, 0};
  if (parameter.type() == ParameterType::Angle)
    return {0, 1};
  return {0, 0};
}
} // namespace curve_expression_detail
} // namespace blcad
