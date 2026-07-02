param(
  [string]$BuildDir = "build/mingw-ninja-release",
  [string]$OutputDir = "examples/output/large_validation_100",
  [int]$Samples = 80
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$exe = Join-Path $root "$BuildDir/bin/linequadrics.exe"
$dataRoot = Join-Path $root "tests/data/external"
$out = if ([IO.Path]::IsPathRooted($OutputDir)) {
  $OutputDir
} else {
  Join-Path $root $OutputDir
}

if (-not (Test-Path -LiteralPath $exe)) {
  throw "linequadrics executable not found: $exe"
}
if (-not (Test-Path -LiteralPath $dataRoot)) {
  throw "External STL data directory not found: $dataRoot"
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
Get-ChildItem -LiteralPath $out -File |
  Where-Object { $_.Extension -in ".stl", ".csv" } |
  Remove-Item -Force

$inputs = @()
$inputs += Get-ChildItem -LiteralPath $dataRoot -Filter "*.stl" -File
$inputs += Get-ChildItem -LiteralPath (Join-Path $dataRoot "large") -Filter "*.stl" -File
$inputs += Get-ChildItem -LiteralPath (Join-Path $dataRoot "thingi10k") -Filter "*.stl" -File
$inputs = $inputs | Sort-Object FullName

if ($inputs.Count -lt 100) {
  throw "Expected at least 100 STL inputs, found $($inputs.Count)."
}

foreach ($input in $inputs) {
  $rel = $input.FullName.Substring($dataRoot.Length + 1)
  $stem = [IO.Path]::GetFileNameWithoutExtension($rel.Replace("\", "_").Replace("/", "_"))
  if ($stem.StartsWith("thingi10k_thingi10k_")) {
    $stem = $stem.Substring("thingi10k_".Length)
  }

  $stl = Join-Path $out ($stem + "_line_090.stl")
  $csv = Join-Path $out ($stem + "_metrics.csv")
  & $exe simplify $input.FullName $stl `
    --method line `
    --ratio 0.90 `
    --line-weight 1e-3 `
    --weight-mode dihedral `
    --feature-boost 0.08 `
    --feature-angle-deg 25 `
    --samples $Samples `
    --metrics-csv $csv
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

$summary = Join-Path $out "summary.csv"
$wrote = $false
Get-ChildItem -LiteralPath $out -Filter "*_metrics.csv" -File |
  Sort-Object FullName |
  ForEach-Object {
    $lines = Get-Content -LiteralPath $_.FullName
    if ($lines.Count -ge 2) {
      if (-not $wrote) {
        Set-Content -LiteralPath $summary -Value ("case," + $lines[0])
        $wrote = $true
      }
      $case = $_.BaseName -replace "_metrics$", ""
      Add-Content -LiteralPath $summary -Value ($case + "," + $lines[1])
    }
  }

Write-Host "Wrote large validation outputs to $out"
