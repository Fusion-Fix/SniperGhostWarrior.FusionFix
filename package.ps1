param(
    [string]$Loader = (Join-Path $PSScriptRoot 'data/winmm.dll')
)
$ErrorActionPreference = 'Stop'
$plugin = Join-Path $PSScriptRoot 'bin/Release/SniperGhostWarrior.FusionFix.asi'
foreach ($file in @($plugin, $Loader)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing package input: $file" }
}
$staging = Join-Path $PSScriptRoot ("build/packages/" + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Force -Path (Join-Path $staging 'plugins') | Out-Null
    Copy-Item -LiteralPath $plugin -Destination (Join-Path $staging 'plugins')
    Copy-Item -LiteralPath $Loader -Destination (Join-Path $staging 'winmm.dll')
    $output = Join-Path $PSScriptRoot 'SniperGhostWarrior.FusionFix.zip'
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($staging, $output)
    Write-Host "Created $output"
} finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
