from pathlib import Path

path = Path("tests/geometry/assembly_generic_relationship_equation_tests.cpp")
text = path.read_text(encoding="utf-8")
text = text.replace("diagnostics.value().jacobian_rank.evaluated", "diagnostics.value().rank_summary.rank_evaluated")
text = text.replace("diagnostics.value().jacobian_rank.rank", "diagnostics.value().rank_summary.jacobian_rank")
text = text.replace("diagnostics.value().jacobian_rank.constrained_dof", "diagnostics.value().rank_summary.constrained_dof")
text = text.replace("diagnostics.value().jacobian_rank.remaining_dof", "diagnostics.value().rank_summary.remaining_dof")
path.write_text(text, encoding="utf-8")

print("Block 39 diagnostics test API normalized")
