param(
  [string]$Config = "Release",
  [double]$Ratio = 0.20,
  [int]$N = 96
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root "build\$Config\linequadrics.exe"
$InputDir = Join-Path $Root "examples\input"
$OutDir = Join-Path $Root "examples\output\feature_curve_validation"

if (!(Test-Path $Exe)) {
  cmake --build (Join-Path $Root "build") --config $Config --target linequadrics
}

New-Item -ItemType Directory -Force $InputDir | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

$Cases = @(
  @{ Name = "stepped_shaft"; Type = "stepped-shaft"; N = $N },
  @{ Name = "pipe_coupling"; Type = "pipe-coupling"; N = $N },
  @{ Name = "pulley"; Type = "pulley"; N = $N },
  @{ Name = "flange_curve"; Type = "flange"; N = [Math]::Max(72, [int]($N * 0.75)) }
)

foreach ($Case in $Cases) {
  $Input = Join-Path $InputDir ($Case.Name + ".stl")
  & $Exe generate --type $Case.Type --n $Case.N --out $Input
  & $Exe feature-report $Input `
    --feature-angle-deg 25 `
    --circle-fit-threshold 0.04 `
    --min-feature-loop-vertices 8 `
    --csv (Join-Path $OutDir ($Case.Name + "_features.csv"))

  $LineOut = Join-Path $OutDir ($Case.Name + "_line.stl")
  $CurveOut = Join-Path $OutDir ($Case.Name + "_curve.stl")

  & $Exe simplify $Input $LineOut `
    --method line `
    --ratio $Ratio `
    --line-weight 1e-3 `
    --weight-mode dihedral `
    --feature-boost 0.08 `
    --feature-angle-deg 25 `
    --samples 1000 `
    --metrics-csv (Join-Path $OutDir ($Case.Name + "_line_metrics.csv"))

  & $Exe simplify $Input $CurveOut `
    --method line `
    --ratio $Ratio `
    --line-weight 1e-3 `
    --weight-mode dihedral `
    --feature-boost 0.08 `
    --feature-angle-deg 25 `
    --preserve-feature-curves `
    --feature-curve-weight 0.08 `
    --circle-fit-threshold 0.04 `
    --min-feature-loop-vertices 16 `
    --samples 1000 `
    --metrics-csv (Join-Path $OutDir ($Case.Name + "_curve_metrics.csv"))

  & $Exe feature-compare $Input $LineOut `
    --feature-angle-deg 25 `
    --circle-fit-threshold 0.04 `
    --min-feature-loop-vertices 8 `
    --csv (Join-Path $OutDir ($Case.Name + "_line_feature_compare.csv"))

  & $Exe feature-compare $Input $CurveOut `
    --feature-angle-deg 25 `
    --circle-fit-threshold 0.04 `
    --min-feature-loop-vertices 8 `
    --csv (Join-Path $OutDir ($Case.Name + "_curve_feature_compare.csv"))
}

Write-Host "Feature-curve validation outputs written to $OutDir"
