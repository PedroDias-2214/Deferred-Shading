#include "Lights.hpp"

#include <cmath>

namespace {

    constexpr float PI = 3.14159265f;
    constexpr float PLANE_HALF = 2.0f;  // o plano vai de -2 a 2 nos eixos X e Y

    float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // Converte um matiz (0 a 1) em RGB com saturação máxima, para as luzes terem cores distintas.
    void hueToRgb(float h, float rgb[3]) {
        rgb[0] = clamp01(std::fabs(h * 6.0f - 3.0f) - 1.0f);
        rgb[1] = clamp01(2.0f - std::fabs(h * 6.0f - 2.0f));
        rgb[2] = clamp01(2.0f - std::fabs(h * 6.0f - 4.0f));
    }

    void setVec4(float dst[4], float a, float b, float c, float d) {
        dst[0] = a;
        dst[1] = b;
        dst[2] = c;
        dst[3] = d;
    }

} // namespace

void SceneLights::build(float time, LightBlock& out) {
    out.lights.clear();
    out.lights.reserve(POINT_COUNT + SPOT_COUNT);

    // ---- Pontuais: grade sobre o plano, cada uma oscilando em volta da sua posição ----
    for (uint32_t i = 0; i < POINT_COUNT; ++i) {
        const float gx = (static_cast<float>(i % GRID) + 0.5f) / GRID * 2.0f - 1.0f;  // -1..1
        const float gy = (static_cast<float>(i / GRID) + 0.5f) / GRID * 2.0f - 1.0f;
        const float phase = static_cast<float>(i) * 0.37f;

        GpuLight l{};
        setVec4(l.positionRadius,
            gx * PLANE_HALF + 0.25f * std::cos(time + phase),
            gy * PLANE_HALF + 0.25f * std::sin(0.7f * time + phase),
            0.4f,    // altura acima do plano
            1.2f);   // raio de alcance

        float rgb[3];
        hueToRgb(static_cast<float>(i) / POINT_COUNT, rgb);
        setVec4(l.colorIntensity, rgb[0], rgb[1], rgb[2], 0.35f);
        setVec4(l.directionType, 0.0f, 0.0f, 0.0f, 1.0f);  // tipo 1 = pontual
        setVec4(l.attenSpot, 1.0f, 0.0f, 4.0f, 0.0f);      // kc, kl, kq
        out.lights.push_back(l);
    }

    // ---- Spot: anel acima do plano, apontando para baixo (-Z) ----
    const float coneInner = std::cos(20.0f * PI / 180.0f);
    const float coneOuter = std::cos(30.0f * PI / 180.0f);
    for (uint32_t i = 0; i < SPOT_COUNT; ++i) {
        const float a = time * 0.5f + static_cast<float>(i) * (2.0f * PI / SPOT_COUNT);

        GpuLight l{};
        setVec4(l.positionRadius, 1.8f * std::cos(a), 1.8f * std::sin(a), 1.2f, 3.0f);
        setVec4(l.colorIntensity, 1.0f, 0.95f, 0.8f, 2.0f);
        setVec4(l.directionType, 0.0f, 0.0f, -1.0f, 2.0f);  // tipo 2 = spot
        setVec4(l.attenSpot, 1.0f, 0.0f, 1.0f, 0.0f);
        setVec4(l.spotCone, coneInner, coneOuter, 0.0f, 0.0f);
        out.lights.push_back(l);
    }

    out.header.lightCount = static_cast<uint32_t>(out.lights.size());
    setVec4(out.header.ambientColor, 0.05f, 0.05f, 0.05f, 0.0f);
}