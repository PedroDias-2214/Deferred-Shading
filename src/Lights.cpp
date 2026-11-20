#include "Lights.hpp"

#include <cmath>

namespace {

    constexpr float PI = 3.14159265f;
    constexpr float GOLDEN_ANGLE = PI * (3.0f - 2.2360680f);

    float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

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

    // Cor da pontual i, conforme a paleta escolhida.
    void pointColor(PointPalette palette, uint32_t i, float rgb[3]) {
        switch (palette) {
        case PointPalette::White:
            rgb[0] = rgb[1] = rgb[2] = 1.0f;
            break;
        case PointPalette::Warm:
            rgb[0] = 1.0f; rgb[1] = 0.6f; rgb[2] = 0.25f;
            break;
        case PointPalette::Cold:
            rgb[0] = 0.4f; rgb[1] = 0.7f; rgb[2] = 1.0f;
            break;
        case PointPalette::Rainbow:
        default:
            hueToRgb(static_cast<float>(i) / SceneLights::POINT_COUNT, rgb);
            break;
        }
    }

} // namespace

void SceneLights::build(float time, const LightSettings& s, LightBlock& out) {
    out.lights.clear();
    out.lights.reserve(POINT_COUNT + SPOT_COUNT);

    // ---- Pontuais: distribuídas quase uniformemente na casca esférica ----
    if (s.points) {
        for (uint32_t i = 0; i < POINT_COUNT; ++i) {
            const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / POINT_COUNT;
            const float r = std::sqrt(1.0f - y * y);
            const float theta = GOLDEN_ANGLE * static_cast<float>(i) + 0.2f * time;

            GpuLight l{};
            setVec4(l.positionRadius,
                SHELL_RADIUS * r * std::cos(theta),
                SHELL_RADIUS * y,
                SHELL_RADIUS * r * std::sin(theta),
                3.0f);

            float rgb[3];
            pointColor(s.palette, i, rgb);
            setVec4(l.colorIntensity, rgb[0], rgb[1], rgb[2], 0.6f * s.intensity);
            setVec4(l.directionType, 0.0f, 0.0f, 0.0f, 1.0f);  // tipo 1 = pontual
            setVec4(l.attenSpot, 1.0f, 0.5f, 1.0f, 0.0f);
            out.lights.push_back(l);
        }
    }

    // ---- Spots: orbitam por fora do modelo, apontando para o centro ----
    if (s.spots) {
        const float coneInner = std::cos(20.0f * PI / 180.0f);
        const float coneOuter = std::cos(30.0f * PI / 180.0f);
        for (uint32_t i = 0; i < SPOT_COUNT; ++i) {
            const float a = 0.5f * time + static_cast<float>(i) * (2.0f * PI / SPOT_COUNT);
            const float px = ORBIT_RADIUS * std::cos(a);
            const float py = 1.0f * std::sin(0.7f * time + static_cast<float>(i) * 1.3f);
            const float pz = ORBIT_RADIUS * std::sin(a);
            const float len = std::sqrt(px * px + py * py + pz * pz);

            GpuLight l{};
            setVec4(l.positionRadius, px, py, pz, 4.0f);
            setVec4(l.colorIntensity, 1.0f, 0.95f, 0.8f, 0.5f * s.intensity);
            setVec4(l.directionType, -px / len, -py / len, -pz / len, 2.0f);  // tipo 2 = spot
            setVec4(l.attenSpot, 1.0f, 0.0f, 0.05f, 0.0f);
            setVec4(l.spotCone, coneInner, coneOuter, 0.0f, 0.0f);
            out.lights.push_back(l);
        }
    }

    out.header.lightCount = static_cast<uint32_t>(out.lights.size());
    setVec4(out.header.ambientColor, 0.05f, 0.05f, 0.05f, 0.0f);
}