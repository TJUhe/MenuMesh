param(
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root "build\$Config\linequadrics.exe"
if (-not (Test-Path $Exe)) {
  $Exe = Join-Path $Root "build\linequadrics.exe"
}
if (-not (Test-Path $Exe)) {
  throw "Build the project first. Example: cmake -S . -B build; cmake --build build --config Release"
}

$InputDir = Join-Path $Root "examples\input"
$OutputDir = Join-Path $Root "examples\output"
New-Item -ItemType Directory -Force $InputDir, $OutputDir | Out-Null

& $Exe generate --type clustered-plane --n 60 --out (Join-Path $InputDir "clustered_plane.stl")
& $Exe generate --type hole-plane --n 60 --out (Join-Path $InputDir "hole_plane.stl")
& $Exe generate --type ridge --n 60 --out (Join-Path $InputDir "ridge.stl")
& $Exe generate --type noisy-plane --n 60 --out (Join-Path $InputDir "noisy_plane.stl")
& $Exe generate --type sine-terrain --n 56 --out (Join-Path $InputDir "sine_terrain.stl")
& $Exe generate --type terrace --n 56 --out (Join-Path $InputDir "terrace.stl")
& $Exe generate --type bump --n 56 --out (Join-Path $InputDir "bump.stl")
& $Exe generate --type cylinder --n 48 --out (Join-Path $InputDir "cylinder.stl")
& $Exe generate --type torus --n 48 --out (Join-Path $InputDir "torus.stl")
& $Exe generate --type cube --n 45 --out (Join-Path $InputDir "cube.stl")
& $Exe generate --type thin-fin --n 48 --out (Join-Path $InputDir "thin_fin.stl")
& $Exe generate --type flange --n 72 --out (Join-Path $InputDir "flange.stl")

& $Exe sweep (Join-Path $InputDir "clustered_plane.stl") (Join-Path $OutputDir "clustered_plane") --ratio 0.12 --weights "0,1e-5,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "clustered_plane.stl") (Join-Path $OutputDir "clustered_plane_boundary") --ratio 0.12 --boundary-weight 5 --weights "0,1e-5,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "hole_plane.stl") (Join-Path $OutputDir "hole_plane_boundary") --ratio 0.15 --boundary-weight 5 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "ridge.stl") (Join-Path $OutputDir "ridge_uniform") --ratio 0.12 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "ridge.stl") (Join-Path $OutputDir "ridge_dihedral") --ratio 0.12 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --weights "1e-3"
& $Exe sweep (Join-Path $InputDir "noisy_plane.stl") (Join-Path $OutputDir "noisy_plane") --ratio 0.12 --weights "0,1e-3,1e-2,1e-1"
& $Exe sweep (Join-Path $InputDir "sine_terrain.stl") (Join-Path $OutputDir "sine_terrain") --ratio 0.15 --weights "0,1e-4,1e-3,1e-2,1e-1"
& $Exe sweep (Join-Path $InputDir "terrace.stl") (Join-Path $OutputDir "terrace_uniform") --ratio 0.15 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "terrace.stl") (Join-Path $OutputDir "terrace_dihedral") --ratio 0.15 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 20 --weights "1e-3"
& $Exe sweep (Join-Path $InputDir "bump.stl") (Join-Path $OutputDir "bump_height") --ratio 0.15 --weight-mode height --feature-boost 0.05 --weights "1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "cylinder.stl") (Join-Path $OutputDir "cylinder") --ratio 0.18 --boundary-weight 2 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "torus.stl") (Join-Path $OutputDir "torus") --ratio 0.18 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "cube.stl") (Join-Path $OutputDir "cube_dihedral") --ratio 0.18 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --weights "1e-3"
& $Exe sweep (Join-Path $InputDir "thin_fin.stl") (Join-Path $OutputDir "thin_fin_uniform") --ratio 0.18 --boundary-weight 2 --weights "0,1e-4,1e-3,1e-2"
& $Exe sweep (Join-Path $InputDir "thin_fin.stl") (Join-Path $OutputDir "thin_fin_dihedral") --ratio 0.18 --boundary-weight 2 --weight-mode dihedral --feature-boost 0.1 --feature-angle-deg 20 --weights "1e-3"
& $Exe sweep (Join-Path $InputDir "flange.stl") (Join-Path $OutputDir "flange_standard_budget") --method standard --ratio 0.15 --weights "0"
& $Exe sweep (Join-Path $InputDir "flange.stl") (Join-Path $OutputDir "flange_line_budget") --method line --ratio 0.15 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --weights "1e-4,1e-3,1e-2"

& $Exe ratio-sweep (Join-Path $InputDir "sine_terrain.stl") (Join-Path $OutputDir "sine_terrain_ratio_line") --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
& $Exe ratio-sweep (Join-Path $InputDir "ridge.stl") (Join-Path $OutputDir "ridge_ratio_line") --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
& $Exe ratio-sweep (Join-Path $InputDir "cube.stl") (Join-Path $OutputDir "cube_ratio_dihedral") --method line --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
& $Exe ratio-sweep (Join-Path $InputDir "flange.stl") (Join-Path $OutputDir "flange_ratio_dihedral") --method line --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
& $Exe face-sweep (Join-Path $InputDir "flange.stl") (Join-Path $OutputDir "flange_face_ladder") --method line --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --faces "1000,900,800,700,600,500,400,300,200,100"

Write-Host "Demo complete. Inspect examples\output\**\metrics.csv and the generated STL files."
