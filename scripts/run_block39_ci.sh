#!/usr/bin/env bash
set -euo pipefail

exec > >(tee /tmp/block39-ci.log) 2>&1

report_failure() {
  status=$?
  if [[ $status -ne 0 ]]; then
    {
      echo 'Block 39 verification failed.'
      echo
      echo '```text'
      tail -n 380 /tmp/block39-ci.log || true
      echo '```'
    } > /tmp/block39-comment.md
    gh pr comment 36 --repo "$GITHUB_REPOSITORY" --body-file /tmp/block39-comment.md || true
  fi
  exit "$status"
}
trap report_failure EXIT

python3 - <<'PY'
from pathlib import Path

path = Path("scripts/apply_block39.py")
text = path.read_text(encoding="utf-8")

numeric_start = text.index(
    'replace_exact(\n    "src/geometry/assembly_constraint_numeric_system.cpp",\n'
    "    '''Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,"
)
numeric_end = text.index(
    "\n# ---------------------------------------------------------------------------\n"
    "# Cross-hierarchy equation descriptor and builder integration.",
    numeric_start,
)
text = text[:numeric_start] + text[numeric_end:]

hierarchy_start = text.index(
    "# ---------------------------------------------------------------------------\n"
    "# Cross-hierarchy equation descriptor and builder integration."
)
hierarchy_end = text.index(
    "# ---------------------------------------------------------------------------\n"
    "# CMake integration.",
    hierarchy_start,
)
text = text[:hierarchy_start] + text[hierarchy_end:]

dev_start = text.index('replace_exact(\n    "docs/development-setup.md",')
dev_end = text.index('replace_exact(\n    "docs/project-goal.md",', dev_start)
text = text[:dev_start] + text[dev_end:]
path.write_text(text, encoding="utf-8")

normalize = Path("scripts/normalize_block39.py")
normalize_text = normalize.read_text(encoding="utf-8")
normalize_text = normalize_text.replace(
    'AssemblyGeometricTargetDescriptor\\nevaluate_descriptor(',
    'AssemblyGeometricTargetDescriptor evaluate_descriptor(',
    1,
)
normalize.write_text(normalize_text, encoding="utf-8")
PY

python3 scripts/apply_block39.py
python3 scripts/normalize_block39_numeric.py
python3 scripts/normalize_block39_hierarchy.py
python3 scripts/normalize_block39_development_setup.py
python3 scripts/normalize_block39_core_tests.py
python3 scripts/normalize_block39.py
python3 scripts/normalize_block39_builder_returns.py
python3 scripts/normalize_block39_diagnostics_tests.py
python3 scripts/normalize_block39_synthetic_targets.py
python3 scripts/normalize_block39_solver_assertions.py

sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2

clang-format -i \
  include/blcad/geometry/assembly_generic_relationship_equation_builder.hpp \
  include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp \
  src/core/assembly_constraint_graph_participation.hpp \
  src/geometry/assembly_generic_relationship_equation_builder.cpp \
  src/geometry/assembly_target_compatibility.cpp \
  src/geometry/assembly_constraint_numeric_system.hpp \
  src/geometry/assembly_constraint_numeric_system.cpp \
  src/geometry/assembly_hierarchy_constraint_equation_builder.cpp \
  tests/core/assembly_generic_relationship_tests.cpp \
  tests/geometry/assembly_generic_relationship_equation_tests.cpp

cmake --workflow --preset dev-build-test

geometry_status=0
cmake --workflow --preset dev-geometry-build-test || geometry_status=$?

focused_status=0
for tag in \
  "[geometry][assembly-generic-relationships]" \
  "[geometry][assembly-generic-relationships-solver]" \
  "[geometry][assembly-generic-relationships-cross-hierarchy]" \
  "[geometry][assembly-generic-relationships-diagnostics]" \
  "[geometry][assembly-target-compatibility]" \
  "[geometry][assembly-cross-hierarchy-target-compatibility]"
do
  echo "===== focused tag: $tag ====="
  ./build/dev-geometry/blcad_geometry_tests "$tag" || focused_status=1
done

if [[ $geometry_status -ne 0 || $focused_status -ne 0 ]]; then
  echo "Geometry workflow status: $geometry_status"
  echo "Focused test status: $focused_status"
  exit 1
fi

rm scripts/normalize_block39.py \
   scripts/normalize_block39_numeric.py \
   scripts/normalize_block39_hierarchy.py \
   scripts/normalize_block39_development_setup.py \
   scripts/normalize_block39_core_tests.py \
   scripts/normalize_block39_builder_returns.py \
   scripts/normalize_block39_diagnostics_tests.py \
   scripts/normalize_block39_synthetic_targets.py \
   scripts/normalize_block39_solver_assertions.py \
   scripts/run_block39_ci.sh

git diff --check
test ! -e scripts/apply_block39.py
test ! -e scripts/normalize_block39.py
test ! -e scripts/normalize_block39_numeric.py
test ! -e scripts/normalize_block39_hierarchy.py
test ! -e scripts/normalize_block39_development_setup.py
test ! -e scripts/normalize_block39_core_tests.py
test ! -e scripts/normalize_block39_builder_returns.py
test ! -e scripts/normalize_block39_diagnostics_tests.py
test ! -e scripts/normalize_block39_synthetic_targets.py
test ! -e scripts/normalize_block39_solver_assertions.py
test ! -e scripts/run_block39_ci.sh
test ! -e .github/workflows/block39-patch.yml
grep -R "AssemblyGenericRelationshipEquationBuilder" -n include src tests
grep -R "assembly-generic-relationships" -n tests docs

git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git add -A
git diff --cached --check
git commit -m "Add generic relationship equations and shared solve integration"
git push origin HEAD:block39-generic-relationship-equations

gh pr comment 36 --repo "$GITHUB_REPOSITORY" --body "Block 39 verification passed: complete Core and Geometry workflows, all four focused generic-relationship tags, and both target-compatibility regression tags succeeded; the verified implementation commit was pushed to the PR branch."
trap - EXIT
