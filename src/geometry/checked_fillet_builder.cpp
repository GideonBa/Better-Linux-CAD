#include "checked_fillet_builder.hpp"
#include "fillet_spine_law.hpp"
#include <ChFiDS_FilSpine.hxx>
#include <ChFiDS_ListIteratorOfListOfStripe.hxx>
#include <ChFiDS_Stripe.hxx>
#include <Law_Composite.hxx>
#include <Standard_ConstructionError.hxx>
#include <TopOpeBRepBuild_HBuilder.hxx>
namespace blcad::geometry::detail {
namespace {
// OCCT 7.6 SetRadius(Law_Function) discards its temporary composite. Install the exact law
// when each elementary spine is appended, including any endpoint continuation needed by caps.
class CheckedSpine final : public ChFiDS_FilSpine {
public:
  CheckedSpine(const ChFiDS_FilSpine& spine, Handle(Law_Function) law,
               std::shared_ptr<std::string> failure)
      : ChFiDS_FilSpine(spine), law_(law), source_(new ChFiDS_Spine(spine)),
        failure_(std::move(failure)) {}
  void AppendElSpine(const Handle(ChFiDS_ElSpine) & el) override {
    ChFiDS_FilSpine::AppendElSpine(el);
    auto& laws = Law(el)->ChangeLaws();
    laws.Clear();
    laws.Append(fillet_spine_law(law_, source_, el, failure_));
  }

private:
  Handle(Law_Function) law_;
  Handle(ChFiDS_Spine) source_;
  std::shared_ptr<std::string> failure_;
};
} // namespace
void CheckedFilletBuilder::install_law(int contour, const Handle(Law_Function) & law) {
  int index = 0;
  for (ChFiDS_ListIteratorOfListOfStripe it(myListStripe); it.More(); it.Next())
    if (++index == contour) {
      auto spine = Handle(ChFiDS_FilSpine)::DownCast(it.Value()->Spine());
      if (spine.IsNull() || law.IsNull())
        Standard_ConstructionError::Raise("Missing Fillet spine or radius law");
      it.Value()->ChangeSpine() = new CheckedSpine(*spine, law, law_failure_);
      return;
    }
  Standard_ConstructionError::Raise("Missing Fillet contour");
}
const TopTools_ListOfShape& CheckedFilletBuilder::Modified(const TopoDS_Shape& face) {
  modified_.Clear();
  for (auto state : {TopAbs_IN, TopAbs_ON, TopAbs_OUT})
    if (Builder()->IsSplit(face, state))
      for (TopTools_ListIteratorOfListOfShape it(Builder()->Splits(face, state)); it.More();
           it.Next())
        modified_.Append(it.Value());
  return modified_;
}
} // namespace blcad::geometry::detail
