param(
  [int]$Port = 5174
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BundledPnpm = "C:\Users\zh\.cache\codex-runtimes\codex-primary-runtime\dependencies\bin\pnpm.cmd"
$Pnpm = if (Test-Path $BundledPnpm) { $BundledPnpm } else { "pnpm" }

Push-Location $Root
try {
  if (-not (Test-Path "node_modules")) {
    & $Pnpm install
  }
  & $Pnpm exec vite --host 127.0.0.1 --port $Port
} finally {
  Pop-Location
}
