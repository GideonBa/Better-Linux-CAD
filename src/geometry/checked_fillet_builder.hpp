#pragma once
#include <ChFi3d_FilBuilder.hxx>
#include <Law_Function.hxx>
#include <memory>
#include <string>
namespace blcad::geometry::detail {
// Uses documented protected stripe/virtual spine extension points; no kernel patch or private
// access.
class CheckedFilletBuilder final : public ChFi3d_FilBuilder {
public:
  using ChFi3d_FilBuilder::ChFi3d_FilBuilder;
  void install_law(int contour, const Handle(Law_Function) &);
  const TopTools_ListOfShape& Modified(const TopoDS_Shape&);
  const std::string& law_failure() const noexcept {
    return *law_failure_;
  }

private:
  TopTools_ListOfShape modified_;
  std::shared_ptr<std::string> law_failure_ = std::make_shared<std::string>();
};
} // namespace blcad::geometry::detail
