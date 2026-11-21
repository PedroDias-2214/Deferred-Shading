$ErrorActionPreference = "Stop"
$build = "out\build\x64-Debug"
$generator = "Visual Studio 18 2026"

# Se a configuração existente foi feita com outro gerador, apaga a pasta de build.
$cache = Join-Path $build "CMakeCache.txt"

if (Test-Path $cache) {
    $homeLine = Select-String -Path $cache -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=" | Select-Object -First 1
    $genLine = Select-String -Path $cache -Pattern "^CMAKE_GENERATOR:INTERNAL=" | Select-Object -First 1
    $projectPath = $PSScriptRoot -replace '\\', '/'

    $pathChanged = $homeLine -and ($homeLine.Line -notmatch [regex]::Escape($projectPath))
    $generatorChanged = $genLine -and ($genLine.Line -notmatch [regex]::Escape($generator))

    if ($pathChanged -or $generatorChanged) {
        Write-Host "Configuracao antiga (outro caminho ou gerador), limpando a pasta de build..."
        Remove-Item -Recurse -Force "out"
    }
}

cmake -G $generator -A x64 -S . -B $build
if ($LASTEXITCODE -ne 0) { throw "Falha na configuracao do CMake" }

cmake --build $build --config Debug
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao" }

& ".\$build\Debug\DeferredVulkan.exe"