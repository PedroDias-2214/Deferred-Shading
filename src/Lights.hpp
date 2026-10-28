#pragma once

#include "SceneData.hpp"

// Gera as luzes da cena para cada quadro. As pontuais formam uma grade sobre o plano,
// e as spot ficam acima dele, apontando para baixo. Todas se movem com o tempo.
class SceneLights {
public:
    static constexpr uint32_t GRID = 8;                    // 8 x 8 = 64 pontuais
    static constexpr uint32_t POINT_COUNT = GRID * GRID;
    static constexpr uint32_t SPOT_COUNT = 8;

    // Preenche out.header e out.lights para o instante 'time', em segundos.
    static void build(float time, LightBlock& out);
};