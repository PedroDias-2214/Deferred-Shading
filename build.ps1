$ErrorActionPreference = "Stop"
$build = "out\build\x64-Debug"

cmake -G "Visual Studio 18 2026" -A x64 -S . -B $build
if ($LASTEXITCODE -ne 0) { throw "Falha na configuração do CMake" }

cmake --build $build --config Debug
if ($LASTEXITCODE -ne 0) { throw "Falha na compilação" }

& ".\$build\Debug\DeferredVulkan.exe"