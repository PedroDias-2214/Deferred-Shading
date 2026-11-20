$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$zip = Join-Path (Split-Path $root -Parent) "Deferred-Shading.zip"
$temp = Join-Path $env:TEMP "Deferred-Shading-pkg"

# Pasta temporária com só o que precisa ser entregue.
Remove-Item -Recurse -Force $temp -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $temp | Out-Null

$items = @("src", "shaders", "assets", "external", "CMakeLists.txt", "build.ps1", "README.md")
foreach ($item in $items) {
    $source = Join-Path $root $item
    if (Test-Path $source) {
        Copy-Item -Recurse -Force $source $temp
    }
}

# Os .spv são gerados pelo build; se existirem, entram no pacote.
$spvDir = Join-Path $root "out\build\x64-Debug\Debug\spv"
if (Test-Path $spvDir) {
    New-Item -ItemType Directory -Force (Join-Path $temp "spv") | Out-Null
    Copy-Item -Force "$spvDir\*" (Join-Path $temp "spv")
}

Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path "$temp\*" -DestinationPath $zip
Remove-Item -Recurse -Force $temp

Write-Host "Pacote criado: $zip"