param(
  [string]$Config = "Release",
  [double]$Ratio = 0.25,
  [int]$Samples = 1000
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root "build\$Config\linequadrics.exe"
$DownloadDir = Join-Path $Root "examples\external\common_3d_test_models"
$OutDir = Join-Path $Root "examples\output\external_model_validation"

if (!(Test-Path $Exe)) {
  cmake --build (Join-Path $Root "build") --config $Config --target linequadrics
}

New-Item -ItemType Directory -Force $DownloadDir | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

$Models = @(
  @{
    Name = "fandisk"
    Url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/fandisk.obj"
    Notes = "CAD-ish benchmark with hard non-circular features"
  },
  @{
    Name = "rocker_arm"
    Url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/rocker-arm.obj"
    Notes = "mechanical scan with holes and irregular tessellation"
  },
  @{
    Name = "beetle"
    Url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/beetle.obj"
    Notes = "small mixed smooth/sharp organic-style model"
  },
  @{
    Name = "cow"
    Url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/cow.obj"
    Notes = "organic model; useful for checking false circular features"
  },
  @{
    Name = "suzanne"
    Url = "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/suzanne.obj"
    Notes = "low-poly hard-edged mesh without true circular CAD loops"
  }
)

function Read-FirstCsvObject([string]$Path) {
  $Lines = Get-Content $Path -TotalCount 2
  if ($Lines.Count -lt 2) {
    return $null
  }
  return ($Lines | ConvertFrom-Csv)[0]
}

function Quote-CsvField($Value) {
  return '"' + ([string]$Value -replace '"', '""') + '"'
}

function Count-ObjTriangleFaces([string]$Path) {
  $Count = 0
  foreach ($Line in Get-Content $Path) {
    if ($Line -match '^\s*f\s+') {
      $Tokens = $Line.Trim() -split '\s+'
      $VertexCount = $Tokens.Count - 1
      if ($VertexCount -ge 3) {
        $Count += $VertexCount - 2
      }
    }
  }
  return $Count
}

$SummaryPath = Join-Path $OutDir "external_summary.csv"
"model,notes,input_path,line_output,curve_output,input_faces,line_faces,curve_faces,line_matched,line_missing,curve_matched,curve_missing,line_rejected_collapses,curve_rejected_collapses" |
  Set-Content -Encoding UTF8 $SummaryPath

foreach ($Model in $Models) {
  $InputRel = "examples/external/common_3d_test_models/$($Model.Name).obj"
  $LineRel = "examples/output/external_model_validation/$($Model.Name)_line.stl"
  $CurveRel = "examples/output/external_model_validation/$($Model.Name)_curve.stl"
  $Input = Join-Path $Root $InputRel
  if (!(Test-Path $Input)) {
    Write-Host "Downloading $($Model.Name) ..."
    Invoke-WebRequest -Uri $Model.Url -OutFile $Input -Headers @{ "User-Agent" = "line-quadrics-qem-validation" }
  }

  $LineOut = Join-Path $Root $LineRel
  $CurveOut = Join-Path $Root $CurveRel
  $FeatureCsv = Join-Path $OutDir ($Model.Name + "_features.csv")
  $LineMetricsCsv = Join-Path $OutDir ($Model.Name + "_line_metrics.csv")
  $CurveMetricsCsv = Join-Path $OutDir ($Model.Name + "_curve_metrics.csv")
  $LineCompareCsv = Join-Path $OutDir ($Model.Name + "_line_feature_compare.csv")
  $CurveCompareCsv = Join-Path $OutDir ($Model.Name + "_curve_feature_compare.csv")

  & $Exe feature-report $Input `
    --feature-angle-deg 35 `
    --circle-fit-threshold 0.05 `
    --min-feature-loop-vertices 8 `
    --csv $FeatureCsv

  & $Exe simplify $Input $LineOut `
    --method line `
    --ratio $Ratio `
    --line-weight 1e-3 `
    --weight-mode dihedral `
    --feature-boost 0.08 `
    --feature-angle-deg 35 `
    --samples $Samples `
    --metrics-csv $LineMetricsCsv

  & $Exe simplify $Input $CurveOut `
    --method line `
    --ratio $Ratio `
    --line-weight 1e-3 `
    --weight-mode dihedral `
    --feature-boost 0.08 `
    --feature-angle-deg 35 `
    --preserve-feature-curves `
    --feature-curve-weight 0.05 `
    --circle-fit-threshold 0.05 `
    --min-feature-loop-vertices 12 `
    --samples $Samples `
    --metrics-csv $CurveMetricsCsv

  & $Exe feature-compare $Input $LineOut `
    --feature-angle-deg 35 `
    --circle-fit-threshold 0.05 `
    --min-feature-loop-vertices 8 `
    --csv $LineCompareCsv

  & $Exe feature-compare $Input $CurveOut `
    --feature-angle-deg 35 `
    --circle-fit-threshold 0.05 `
    --min-feature-loop-vertices 8 `
    --csv $CurveCompareCsv

  $LineMetrics = Read-FirstCsvObject $LineMetricsCsv
  $CurveMetrics = Read-FirstCsvObject $CurveMetricsCsv
  $LineCompare = Read-FirstCsvObject $LineCompareCsv
  $CurveCompare = Read-FirstCsvObject $CurveCompareCsv
  $InputFaces = Count-ObjTriangleFaces $Input

  $SummaryFields = @(
    $Model.Name,
    $Model.Notes,
    $InputRel,
    $LineRel,
    $CurveRel,
    $InputFaces,
    $LineMetrics.faces,
    $CurveMetrics.faces,
    $LineCompare.matched,
    $LineCompare.missing,
    $CurveCompare.matched,
    $CurveCompare.missing,
    $LineMetrics.rejected_collapses,
    $CurveMetrics.rejected_collapses
  )
  (($SummaryFields | ForEach-Object { Quote-CsvField $_ }) -join ",") |
    Add-Content -Encoding UTF8 $SummaryPath
}

Write-Host "External model validation outputs written to $OutDir"
