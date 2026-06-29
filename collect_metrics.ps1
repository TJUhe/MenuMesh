param(
  [string]$OutputDir = "examples\output"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$ResolvedOutput = Join-Path $Root $OutputDir

if (-not (Test-Path $ResolvedOutput)) {
  throw "Output directory not found: $ResolvedOutput"
}

$Rows = foreach ($File in Get-ChildItem -Path $ResolvedOutput -Recurse -Filter metrics.csv) {
  $CaseName = Split-Path -Leaf $File.DirectoryName
  Import-Csv $File.FullName | ForEach-Object {
    if (-not ($_.PSObject.Properties.Name -contains "ratio")) {
      $_ | Add-Member -NotePropertyName ratio -NotePropertyValue ""
    }
    if (-not ($_.PSObject.Properties.Name -contains "target_faces")) {
      $_ | Add-Member -NotePropertyName target_faces -NotePropertyValue ""
    }
    $_ | Add-Member -NotePropertyName case -NotePropertyValue $CaseName -PassThru
  }
}

$Summary = Join-Path $ResolvedOutput "demo_summary.csv"
$Rows | Export-Csv $Summary -NoTypeInformation
Write-Host "Wrote $Summary"
