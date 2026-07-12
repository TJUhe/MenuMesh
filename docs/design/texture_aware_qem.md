# Texture-aware QEM without matrix expansion

## Decision

ManuMesh keeps its geometry quadric as `Mat4`, evaluated on homogeneous
positions `(x, y, z, 1)`. Texture coordinates are not appended to that vector
and do not enlarge the placement solve.

For an edge-collapse placement `p`, the queue uses

```text
E_total(p) = E_geometry_4x4(p) + textureWeight * E_uv_local(p)
```

where `E_uv_local` is a scalar evaluated only on the faces already touched by
the collapse. The placement candidates remain the existing endpoint,
midpoint, and stable 3D QEM optimum candidates.

## UV ownership

UVs are stored per face corner in `Mesh::faceTexCoords`. A vertex-owned UV
cannot represent a texture seam because one geometric vertex may belong to
several UV charts. OBJ `vt` indices are therefore retained independently for
each triangulated face corner.

The face-corner array is either empty or aligned with `Mesh::faces`. An aligned
entry can be invalid when an OBJ contains an untextured face. Transitions
between textured and untextured incident faces are treated as protected chart
boundaries.

## Local chart policy

For each collapse endpoint, incident corner UVs are grouped with a
tolerance-grid hash. The faces incident to the collapsed edge define a
one-to-one pairing between the endpoint charts.

A collapse is rejected when:

- one endpoint chart has no corresponding chart at the other endpoint;
- the pairing is ambiguous or merges unrelated charts;
- a surviving UV triangle changes signed orientation; or
- a surviving UV triangle falls below `minTextureAreaRatio` of its old signed
  area.

This permits a collapse along a two-sided seam when both chart sides pair
consistently. It blocks a collapse across a seam or away from a seam vertex
when that operation would merge chart ownership.

## Scalar distortion cost

For a compatible collapse, each paired chart receives a linearly interpolated
UV at the 3D edge parameter of `p`. The local cost is

```text
E_uv_local = edgeLength^2
             * sum(faceArea * cornerUvDisplacement^2)
             / meanLocalUvEdgeLength^2
```

The face-area and edge-length factors give the term the same length-power as
the area-weighted geometry QEM. Division by local UV edge scale makes the
result invariant to uniform rescaling of the UV atlas.

After an accepted collapse, only affected surviving face corners are updated.
Different paired charts retain different merged UVs at the same geometric
vertex.

## Complexity

The geometry solve remains a fixed 3D solve backed by a 4x4 homogeneous
quadric. Texture work uses expected O(k) chart hashing and triangle checks for
the local one-ring size `k`, followed by the existing priority-queue work.
There is no global parameterization, atlas traversal, or attribute-space
matrix factorization, so the edge-collapse asymptotic complexity is unchanged.

## Controls and diagnostics

- `preserveTexture`: enables chart and signed-area protection when UV data is
  present; default `false`, so callers must opt in explicitly.
- `textureWeight`: scales only the local scalar ranking cost.
- `textureSeamTolerance`: relative tolerance for local chart grouping.
- `minTextureAreaRatio`: hard lower bound for surviving signed UV area.
- `textureProtectedEdges`: initial edges with no valid midpoint texture
  collapse.
- `textureRejectedCollapses`: current queue candidates rejected by texture
  checks after placement evaluation.

The default `preserveTexture = false` leaves candidate ranking and geometry
output identical to the legacy untextured path. Face-corner UVs are still
propagated, but no distortion or seam guarantee is applied.

The optional fixed-topology quality-refinement round is currently skipped for
textured inputs while protection is active, because that relocation stage does
not yet optimize or constrain UV distortion.

## Literature relation

Garland and Heckbert's attribute-aware work is the historical reference for
including color and texture in simplification objectives (M003). ManuMesh uses
the alternative engineering split suggested by the current edge-collapse
pipeline literature (M033): geometry QEM ranks fixed 3D placements, while
topology, feature, and attribute validity remain explicit local policies. This
also stays compatible with the 4x4 line-quadric backbone (M004/085).
