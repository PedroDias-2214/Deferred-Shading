#pragma once

#include "SceneData.hpp"

// Gera as luzes da cena para cada quadro. As pontuais ficam em uma casca esférica
// em volta do modelo, e as spot orbitam por fora, apontando para o centro.
class SceneLights {
public:
    static constexpr uint32_t POINT_COUNT = 64;
    static constexpr uint32_t SPOT_COUNT = 8;
    static constexpr float SHELL_RADIUS = 2.2f;  // distância das pontuais ao centro (o modelo tem raio 1)
    static constexpr float ORBIT_RADIUS = 2.6f;  // distância das spots ao centro

    // Preenche out.header e out.lights para o instante 'time', em segundos.
    static void build(float time, LightBlock& out);
};