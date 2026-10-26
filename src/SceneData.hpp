#pragma once

#include <cstdint>

// Número máximo de luzes no SSBO. Define o tamanho do buffer de luzes.
inline constexpr uint32_t MAX_LIGHTS = 1024;

// Espelha o bloco CameraUBO do shader (std140). As matrizes são guardadas
// em ordem de colunas, como o GLSL espera.
struct CameraData {
    float view[16];
    float proj[16];
    float viewProj[16];
    float invView[16];
    float invProj[16];
    float cameraPos[4];
    float screenSize[2];
    float nearPlane;
    float farPlane;
};
static_assert(sizeof(CameraData) == 352, "CameraData deve ter 352 bytes (std140)");

// Cabeçalho do SSBO de luzes (std430). As luzes vêm logo depois, a partir do byte 32.
struct LightHeader {
    uint32_t lightCount;
    uint32_t pad[3];
    float ambientColor[4];
};
static_assert(sizeof(LightHeader) == 32, "LightHeader deve ter 32 bytes");

// Uma luz no formato do shader (5 x vec4).
struct GpuLight {
    float positionRadius[4];   // xyz = posição, w = raio
    float colorIntensity[4];   // rgb = cor, a = intensidade
    float directionType[4];    // xyz = direção, w = tipo (0 direcional, 1 pontual, 2 spot)
    float attenSpot[4];        // x = kc, y = kl, z = kq
    float spotCone[4];         // x = cos interno, y = cos externo
};
static_assert(sizeof(GpuLight) == 80, "GpuLight deve ter 80 bytes");

// Matriz identidade em ordem de colunas.
inline void setIdentity(float m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}