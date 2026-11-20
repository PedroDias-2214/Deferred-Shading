#pragma once

#include "SceneData.hpp"

// Paleta de cores das pontuais. Trocada pela tecla C.
enum class PointPalette : uint32_t { Rainbow, White, Warm, Cold, Count };

// Controles de luz, alterados em tempo real pelo teclado.
struct LightSettings {
    bool points = true;                            // tecla L
    bool spots = true;                             // tecla K
    PointPalette palette = PointPalette::Rainbow;  // tecla C
    float intensity = 1.0f;                        // teclas = e -
};

// Gera as luzes da cena para cada quadro. As pontuais ficam em uma casca esférica
// em volta do modelo, e os spots orbitam por fora, apontando para o centro.
class SceneLights {
public:
    static constexpr uint32_t POINT_COUNT = 64;
    static constexpr uint32_t SPOT_COUNT = 8;
    static constexpr float SHELL_RADIUS = 2.2f;
    static constexpr float ORBIT_RADIUS = 4.0f;

    // Preenche out com as luzes ativas em 'settings' para o instante 'time', em segundos.
    static void build(float time, const LightSettings& settings, LightBlock& out);
};