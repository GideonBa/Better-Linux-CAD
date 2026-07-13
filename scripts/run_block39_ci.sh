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
      tail -n 140 /tmp/block39-ci.log || true
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
start = text.index(
    'replace_exact(\n    "src/geometry/assembly_constraint_numeric_system.cpp",\n'
    "    '''Result<std::size_t> append_scaled_residuals(const InsertResidualDescriptor& residual,"
)
end = text.index(
    "\n# ---------------------------------------------------------------------------\n"
    "# Cross-hierarchy equation descriptor and builder integration.",
    start,
)
path.write_text(text[:start] + text[end:], encoding="utf-8")
PY

python3 scripts/apply_block39.py
python3 scripts/normalize_block39_numeric.py
python3 scripts/normalize_block39.py

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
  tests/geometry/assembly_generic_relationship_equation_tests.cpp

cmake --workflow --preset dev-build-test
cmake --workflow --preset dev-geometry-build-test

./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"

rm scripts/normalize_block39.py scripts/normalize_block39_numeric.py scripts/run_block39_ci.sh

git diff --check
test ! -e scripts/apply_block39.py
test ! -e scripts/normalize_block39.py
test ! -e scripts/normalize_block39_numeric.py
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
