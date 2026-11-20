$ErrorActionPreference = "Stop"
$build = "out\build\x64-Debug"
$generator = "Visual Studio 18 2026"

# Se a configuração existente foi feita com outro gerador, apaga a pasta de build.
$cache = Join-Path $build "CMakeCache.txt"
if (Test-Path $cache) {
    $line = Select-String -Path $cache -Pattern "^CMAKE_GENERATOR:INTERNAL=" | Select-Object -First 1
    if ($line -and $line.Line -notmatch [regex]::Escape($generator)) {
        Write-Host "Gerador antigo encontrado, limpando a pasta de build..."
        Remove-Item -Recurse -Force "out"
    }
}

cmake -G $generator -A x64 -S . -B $build
if ($LASTEXITCODE -ne 0) { throw "Falha na configuracao do CMake" }

cmake --build $build --config Debug
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao" }

& ".\$build\Debug\DeferredVulkan.exe"