# Baixa as bibliotecas de cabeçalho usadas pelo projeto (glTF e imagens).
# Os arquivos vão para ..\ThirdParty\headers, ao lado do projeto, como o GLFW.
$ErrorActionPreference = "Stop"

$headersDir = Join-Path $PSScriptRoot "..\ThirdParty\headers"
New-Item -ItemType Directory -Force $headersDir | Out-Null

$deps = @(
    @{ Name = "tiny_gltf.h";  Url = "https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h" },
    @{ Name = "json.hpp";     Url = "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" },
    @{ Name = "stb_image.h";  Url = "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" }
)

foreach ($dep in $deps) {
    $target = Join-Path $headersDir $dep.Name
    if (Test-Path $target) {
        Write-Host "Já existe: $($dep.Name)"
        continue
    }
    Write-Host "Baixando $($dep.Name)..."
    Invoke-WebRequest -Uri $dep.Url -OutFile $target
}

Write-Host "Bibliotecas prontas em $headersDir"