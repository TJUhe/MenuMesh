# Large Thingi10K fixtures

The large fixture bundle is optional and is not part of the source checkout.
For an offline build, obtain the approved bundle from the internal artifact
store and stage it at `output/thingi10k_large/`. The directory must contain
the binary STL files named by `manifest.json`; each model record must include
its face count and SHA-256 digest. Do not add an external acquisition step to
the build or test workflow.

The `thingi10k_manifest` CTest validates the local files and generates the
derived `manifest.index` consumed by the external test. If the manifest is not
present, the validation test passes as a no-op and the large external test is
skipped. This keeps normal configure, build, and test runs fully offline.

The bundle currently used by the validation team contains two closed
edge-manifold meshes and one closed non-manifold mesh. Each model retains the
license recorded in its internal manifest; review those fields before
redistributing any fixture.
