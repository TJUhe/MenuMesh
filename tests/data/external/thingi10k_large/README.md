# Large Thingi10K fixtures

Use `tests/support/fetch_thingi10k_large.py` to download local STL fixtures:

```text
python tests/support/fetch_thingi10k_large.py --min-faces 2000000 --limit 3
```

The default output is `output/thingi10k_large/`, which is ignored by Git.  The
script writes `manifest.json` with the source URL, per-model license, triangle
count, byte count, and SHA-256 digest.  The dataset repository is Apache-2.0,
but each model retains the license recorded in Thingi10K metadata; review that
field before redistributing any downloaded mesh.

The default selection currently includes two closed edge-manifold meshes and
one closed non-manifold mesh.  This gives the large-mesh validation path both a
clean capacity case and a topology-stress case.  Existing downloads are hashed
incrementally and binary STL header/count consistency is checked before the
manifest is replaced.
