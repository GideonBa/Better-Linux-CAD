from pathlib import Path

path = Path("tests/geometry/assembly_generic_relationship_equation_tests.cpp")
text = path.read_text(encoding="utf-8")

old_local = '''  const auto& proposal = local_proposal(solved.value());
  CHECK(proposal.source_transform == source);
  CHECK(proposal.proposed_transform.translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(0.0).margin(kTolerance));
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == source);

'''
new_local = '''  const auto& proposal = local_proposal(solved.value());
  CHECK(proposal.source_transform == source);
  CHECK_FALSE(proposal.proposed_transform == source);
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == source);

  Project applied_project = project;
  const AssemblySolveResultApplier local_applier;
  auto local_applied = local_applier.apply(applied_project, solved.value());
  REQUIRE(local_applied);
  CHECK(local_applied.value() == 1U);
  auto verified = solver.solve(applied_project, local_group());
  REQUIRE(verified);
  REQUIRE(verified.value().converged());
  CHECK(verified.value().residual_summary.initial_rms <= 1.0e-8);

'''
if old_local not in text:
    raise RuntimeError("local transform-specific solver assertion marker not found")
text = text.replace(old_local, new_local, 1)

old_cross_proposal = '''  REQUIRE(solved.value().proposed_transforms.size() == 1U);
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.x ==
        Approx(0.0).margin(kTolerance));
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.y ==
        Approx(0.0).margin(kTolerance));
  CHECK(solved.value().proposed_transforms.front().proposed_transform.translation_mm.z ==
        Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project) == source);

'''
new_cross_proposal = '''  REQUIRE(solved.value().proposed_transforms.size() == 1U);
  CHECK(solved.value().proposed_transforms.front().source_transform == source);
  CHECK_FALSE(solved.value().proposed_transforms.front().proposed_transform == source);
  CHECK(child_transform(project) == source);

'''
if old_cross_proposal not in text:
    raise RuntimeError("cross transform-specific proposal assertion marker not found")
text = text.replace(old_cross_proposal, new_cross_proposal, 1)

old_cross_apply = '''  auto applied = applier.apply(project, solved.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(child_transform(project).translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project).translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(child_transform(project).translation_mm.z == Approx(0.0).margin(kTolerance));
'''
new_cross_apply = '''  auto applied = applier.apply(project, solved.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  auto verified = solver.solve(project, graph.value().solve_groups().front());
  REQUIRE(verified);
  REQUIRE(verified.value().converged());
  CHECK(verified.value().residual_summary.initial_rms <= 1.0e-8);
'''
if old_cross_apply not in text:
    raise RuntimeError("cross transform-specific apply assertion marker not found")
text = text.replace(old_cross_apply, new_cross_apply, 1)

path.write_text(text, encoding="utf-8")
print("Block 39 solver assertions normalized to residual-state verification")
