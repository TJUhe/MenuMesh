# Test Data Layout

Small deterministic fixtures and larger external validation meshes.

| Path | Purpose |
| --- | --- |
| `feature_fixtures/` | Small handcrafted OBJ fixtures for focused feature-detection tests. |
| `qem_test/` | Case-list files used by dataset and parameterized QEM tests. |
| `external/` | Tracked external STL validation meshes, including larger regression inputs. |

New unit-test geometry belongs in `feature_fixtures/` or a focused subdirectory
under `qem_test/`. Large or third-party meshes belong in `external/` when they
are project regression assets.
