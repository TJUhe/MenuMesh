# Documentation Layout

Documentation root for the project.

## Documentation Policy

Docs must follow the program, not the other way around.

- CLI examples must match `apps/linequadrics/main.cpp` and should be checked
  with the built `linequadrics --help` plus at least one real run when a command
  changes.
- Algorithm explanations must describe the current implementation in
  `src/simplification/QEMSimplifier.cpp`, `src/features/FeatureDetection.cpp`,
  the private helpers in `src/simplification/detail/`, and the public
  options in `include/line_quadrics_qem/`.
- Paper notes should separate source ideas from implemented behavior. If a
  paper suggests a technique that is not implemented, say so explicitly.
- Generated HTML notes under `generated/notes/` are still reference artifacts,
  but the claims inside them should stay consistent with the current CLI,
  tests, and source layout.

| Path | Purpose |
| --- | --- |
| `design/` | Architecture notes, algorithm design, validation plans, and roadmaps. |
| `design/source_organization.md` | Public/private source layout policy and module growth rules. |
| `guide/` | User-facing and developer-facing usage guides. |
| `papers/` | Canonical paper PDF archive and paper index. |
| `generated/notes/` | Exported HTML/PDF/ZIP notes and browsable reports. |
| `Doxyfile.in` | Doxygen configuration template for generated API reference. |

Long-lived design decisions belong in `design/`, operational how-to material in
`guide/`, and paper PDFs in `papers/`.
