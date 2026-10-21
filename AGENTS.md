# Repository working agreement

- Complete authorized work with reviewed code, appropriate tests and updated documentation.
- Keep behavior, persistence and UI contracts in the relevant existing `docs/` files in sync with
  the implementation. `docs/mvp-plan.md` owns numbered implementation status; do not advance an
  entire block merely because individual fixes have shipped.
- Follow `docs/development-setup.md` for builds and checks. Use focused regression tests for fixes;
  changes to rendering or interaction also need native Qt/OCCT verification when available. Report
  any skipped or unavailable checks accurately.
- Inspect the diff and staged content. Preserve unrelated work, keep commits focused, and use
  descriptive imperative commit messages with useful validation details.
- After completing and verifying each coherent change, commit it and push to the configured
  upstream. The user has explicitly authorized this workflow. Use normal pushes; do not overwrite
  remote history. Honor any later instruction that limits or postpones committing or publishing.
