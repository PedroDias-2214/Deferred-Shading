// =====================================================================
//  lighting.frag  —  FRAGMENT SHADER do LIGHTING PASS (Subpass 1)
// ---------------------------------------------------------------------
//  Tema: Deferred Shading  |  Entrega 3 — Pseudocódigo dos Shaders
//
//  OBJETIVO
//    Executado UMA vez por pixel da tela (triângulo do fullscreen.vert).
//    Lê os atributos da superfície gravados no G-Buffer pelo Geometry
//    Pass, percorre a lista de luzes da cena (SSBO) e acumula a
//    iluminação de Blinn-Phong de cada luz. Por fim aplica exposição,
//    tone mapping e correção gama, produzindo a COR FINAL do pixel.
//
//  FLUXO DE DADOS
//    G-Buffer (Set 3, input attachments do Subpass 0)
//    + SSBO de luzes (Set 2) + UBO Câmera (Set 0) + Push Constants
//    ──► [lighting.frag]  ──►  location 0: cor final (attachment de saída
//                             do Subpass 1, depois transicionado para
//                             PRESENT_SRC_KHR e apresentado)
//
//  EQUAÇÃO GERAL (por pixel p, com L luzes):
//      C(p) = I_amb · kd  +  Σ_{i=1..L}  att_i · spot_i · c_i · e_i ·
//             [ kd · max(N·L_i, 0)  +  ks · max(N·H_i, 0)^α ]
// =====================================================================

#version 450

// ---------------------------------------------------------------------
//  1. ENTRADA DO VERTEX SHADER
// ---------------------------------------------------------------------
layout(location = 0) in vec2 vUV;    // coordenada do pixel em [0,1]² (usada só no modo debug/reconstrução)

// ---------------------------------------------------------------------
//  2. G-BUFFER — Descriptor Set 3, tipo VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
//
//  subpassInput NÃO é uma textura comum:
//   • não tem sampler, não tem filtro, não tem mipmap, não aceita UV;
//   • subpassLoad() lê SOMENTE o texel do pixel atual (gl_FragCoord.xy);
//   • por isso a leitura é exata (sem interpolação que misturaria
//     normais/posições de objetos diferentes) e, em GPUs tile-based
//     (celulares), os dados podem nem sair da memória on-chip.
//  input_attachment_index = posição em pInputAttachments do Subpass 1.
// ---------------------------------------------------------------------
layout(input_attachment_index = 0, set = 3, binding = 0) uniform subpassInput gPosition;   // RGBA16F: xyz mundo, w marcador
layout(input_attachment_index = 1, set = 3, binding = 1) uniform subpassInput gNormal;     // RGBA16F: xyz normal, w brilho/256
layout(input_attachment_index = 2, set = 3, binding = 2) uniform subpassInput gAlbedoSpec; // RGBA8  : rgb albedo, a especular
layout(input_attachment_index = 3, set = 3, binding = 3) uniform subpassInput gDepth;      // D32    : profundidade [0,1]

// ---------------------------------------------------------------------
//  3. UBO DA CÂMERA — Set 0, Binding 0 (o MESMO do Geometry Pass)
// ---------------------------------------------------------------------
layout(set = 0, binding = 0, std140) uniform CameraUBO {
    mat4  view;
    mat4  proj;
    mat4  viewProj;
    mat4  invView;
    mat4  invProj;
    vec4  cameraPos;     // xyz = posição do observador no mundo (usada no vetor V)
    vec2  screenSize;
    float nearPlane;
    float farPlane;
} camera;

// ---------------------------------------------------------------------
//  4. SSBO DE LUZES — Set 2, Binding 0 (VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
//     Layout std430. Tamanho variável: um array "sem tamanho" no final
//     permite qualquer número de luzes, limitado pelo buffer alocado.
//     Cada Light ocupa 5 × vec4 = 80 bytes (múltiplo de 16 bytes, sem padding extra).
// ---------------------------------------------------------------------
const uint LIGHT_DIRECTIONAL = 0u;   // sol: direção fixa, sem atenuação
const uint LIGHT_POINT       = 1u;   // lâmpada: emite em todas as direções
const uint LIGHT_SPOT        = 2u;   // lanterna: cone com ângulos interno/externo

struct Light {
    vec4 positionRadius;   // xyz = posição no mundo | w = raio de alcance r (distância onde a luz vale 0)
    vec4 colorIntensity;   // rgb = cor c (linear)    | a = intensidade e (escalar, pode ser > 1: HDR)
    vec4 directionType;    // xyz = direção para onde a luz aponta (spot/direcional) | w = tipo (0,1,2)
    vec4 attenSpot;        // x = kc (constante), y = kl (linear), z = kq (quadrática), w não usado
    vec4 spotCone;         // x = cos(ângulo interno), y = cos(ângulo externo), zw não usados
};

layout(set = 2, binding = 0, std430) readonly buffer LightSSBO {
    uint  lightCount;      // número de luzes válidas neste frame
    uint  _pad0, _pad1, _pad2;   // preenchimento: o array começa em offset 16
    vec4  ambientColor;    // rgb = luz ambiente global I_amb | a não usado
    Light lights[];        // array de tamanho dinâmico
} lightData;

// ---------------------------------------------------------------------
//  5. PUSH CONSTANTS do Lighting Pass (parâmetros de pós-processamento)
// ---------------------------------------------------------------------
layout(push_constant) uniform LightingPC {
    float exposure;        // exposição da câmera (ex.: 1.0); multiplica a cor HDR
    uint  debugView;       // 0 = final | 1 = posição | 2 = normal | 3 = albedo | 4 = especular | 5 = profundidade
    uint  maxLights;       // limite de segurança ((tamanho do SSBO − 32 bytes de cabeçalho) / 80 bytes)
    uint  _pad;
    vec4  backgroundColor; // cor dos pixels SEM geometria (céu/fundo)
} pc;

// Constante de especialização: 1 se a swapchain NÃO for _SRGB (o shader
// aplica a gama); 0 se for _SRGB (o hardware já converte na escrita).
layout(constant_id = 0) const uint APPLY_GAMMA = 1u;

// ---------------------------------------------------------------------
//  6. SAÍDA
// ---------------------------------------------------------------------
layout(location = 0) out vec4 outColor;   // cor final do pixel (rgb) | a = 1

// ---------------------------------------------------------------------
//  CONSTANTES
// ---------------------------------------------------------------------
const float EPSILON       = 1e-4;
const float MAX_SHININESS = 256.0;
const float GAMMA         = 2.2;
const uint  HARD_LIGHT_LIMIT = 1024u;   // teto absoluto do loop (proteção contra lightCount corrompido)

// =====================================================================
//  FUNÇÕES AUXILIARES
// =====================================================================

// Atenuação por distância de luzes pontuais/spot.
//  (a) Modelo clássico:          f(d) = 1 / (kc + kl·d + kq·d²)
//  (b) Janela suave até o raio:  w(d) = clamp(1 − (d/r)⁴, 0, 1)²
//  O produto f·w garante que a luz chegue EXATAMENTE a 0 em d = r,
//  o que torna correto ignorar pixels fora do raio (PASSO 4.2).
float computeAttenuation(float d, float radius, vec3 k) {
    float denom = k.x + k.y * d + k.z * d * d;
    denom       = max(denom, EPSILON);                  // evita divisão por zero se kc=kl=kq=0
    float f     = 1.0 / denom;

    float ratio  = d / max(radius, EPSILON);            // raio 0 => luz praticamente nula
    float ratio4 = ratio * ratio * ratio * ratio;
    float window = clamp(1.0 - ratio4, 0.0, 1.0);
    return f * window * window;
}

// Fator do cone de uma luz spot.
//  θ = ângulo entre o eixo da lanterna e o raio luz→pixel.
//  cosθ = dot(−L, D).  Transição suave entre cone interno e externo:
//      spot = smoothstep(cosOuter, cosInner, cosθ)
//  (1 dentro do cone interno, 0 fora do externo, suave no meio)
float computeSpot(vec3 L, vec3 spotDir, float cosInner, float cosOuter) {
    float cosTheta = dot(-L, spotDir);
    // Caso inválido: cosInner <= cosOuter faria smoothstep indefinido.
    if (cosInner <= cosOuter + EPSILON) {
        return (cosTheta >= cosOuter) ? 1.0 : 0.0;      // cone "duro"
    }
    return smoothstep(cosOuter, cosInner, cosTheta);
}

// Tone mapping ACES (aproximação de Narkowicz, 2015):
//  comprime HDR [0, ∞) para [0, 1) preservando contraste.
//      T(x) = (x(2.51x + 0.03)) / (x(2.43x + 0.59) + 0.14)
vec3 tonemapACES(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Verifica NaN/Inf componente a componente (valores inválidos que
// podem surgir de dados corrompidos no G-Buffer ou no SSBO).
bool isInvalid(vec3 v) {
    return any(isnan(v)) || any(isinf(v));
}

// =====================================================================
//  MAIN
// =====================================================================
void main() {
    // =================================================================
    //  PASSO 1 — LEITURA DO G-BUFFER (4 leituras, sem filtragem)
    // =================================================================
    vec4  posSample    = subpassLoad(gPosition);
    vec4  normalSample = subpassLoad(gNormal);
    vec4  albedoSample = subpassLoad(gAlbedoSpec);
    float depth        = subpassLoad(gDepth).r;

    vec3  P          = posSample.xyz;                    // posição da superfície no mundo
    vec3  kd         = albedoSample.rgb;                 // coeficiente difuso  = albedo (linear)
    float ks         = albedoSample.a;                   // coeficiente especular [0,1]
    float alpha      = max(normalSample.w * MAX_SHININESS, 1.0);  // expoente de brilho α ∈ [1,256]

    // =================================================================
    //  PASSO 2 — CASO EXTREMO: pixel SEM geometria (fundo/céu)
    //  O G-Buffer foi limpo com w = 0 e depth = 1.0 (plano distante).
    //  Se nenhum objeto escreveu nesse pixel, não há o que iluminar.
    // =================================================================
    if (posSample.w < 0.5 || depth >= 1.0) {
        outColor = vec4(pc.backgroundColor.rgb, 1.0);
        return;                                          // saída antecipada: economiza o loop de luzes
    }

    // =================================================================
    //  PASSO 3 — VETORES BASE (todos normalizados, espaço de MUNDO)
    // =================================================================
    // N: normal da superfície. Já foi normalizada no Geometry Pass, mas
    // a precisão de 16 bits introduz erro; re-normalizamos.
    float nLen2 = dot(normalSample.xyz, normalSample.xyz);
    if (nLen2 < EPSILON) {
        // Normal inválida (vetor nulo): ilumina só com ambiente.
        outColor = vec4(lightData.ambientColor.rgb * kd, 1.0);
        return;
    }
    vec3 N = normalSample.xyz * inversesqrt(nLen2);

    // V: vetor da superfície até o observador.   V = normalize(C − P)
    vec3 toCamera = camera.cameraPos.xyz - P;
    vec3 V = (dot(toCamera, toCamera) > EPSILON) ? normalize(toCamera) : N;

    // =================================================================
    //  PASSO 4 — LOOP SOBRE TODAS AS LUZES (núcleo do Deferred Shading)
    //
    //  Critério de parada: i < numLights, onde numLights é o MENOR entre
    //  o valor informado pela CPU, o tamanho real do buffer e um teto
    //  fixo. Isso impede leitura fora do SSBO (acesso inválido).
    // =================================================================
    uint numLights = min(lightData.lightCount, min(pc.maxLights, HARD_LIGHT_LIMIT));

    vec3 Lo = vec3(0.0);                                 // radiância acumulada de todas as luzes

    for (uint i = 0u; i < numLights; ++i) {
        Light light = lightData.lights[i];
        uint  type  = uint(light.directionType.w);

        vec3  L;                                         // direção superfície -> luz (normalizada)
        float attenuation = 1.0;                         // fator de distância
        float spotFactor  = 1.0;                         // fator de cone

        // -------------------------------------------------------------
        //  4.1 — Direção da luz L e atenuação, por tipo
        // -------------------------------------------------------------
        if (type == LIGHT_DIRECTIONAL) {
            // Luz no infinito: L é constante e oposto à direção do raio.
            // Sem atenuação (attenuation = 1).
            vec3 dir = light.directionType.xyz;
            if (dot(dir, dir) < EPSILON) continue;       // direção inválida -> ignora a luz
            L = normalize(-dir);
        } else {
            vec3  toLight = light.positionRadius.xyz - P;
            float d2      = dot(toLight, toLight);       // distância ao quadrado
            float radius  = light.positionRadius.w;

            // 4.2 — CULLING POR RAIO: fora do alcance a contribuição é 0,
            // então pulamos todos os cálculos restantes (otimização).
            if (d2 > radius * radius) continue;
            // Caso extremo: pixel exatamente na posição da luz.
            if (d2 < EPSILON) continue;

            float d = sqrt(d2);
            L = toLight / d;                             // normalização reaproveitando d
            attenuation = computeAttenuation(d, radius, light.attenSpot.xyz);

            if (type == LIGHT_SPOT) {
                vec3 spotDir = light.directionType.xyz;
                if (dot(spotDir, spotDir) < EPSILON) continue;
                spotFactor = computeSpot(L, normalize(spotDir),
                                         light.spotCone.x, light.spotCone.y);
            } else if (type != LIGHT_POINT) {
                continue;                                // tipo desconhecido -> ignora
            }
        }

        // Contribuição nula: não precisa calcular o resto.
        if (attenuation * spotFactor <= 0.0) continue;

        // -------------------------------------------------------------
        //  4.3 — DIFUSO de Lambert
        //      diff = max(N·L, 0)
        //  N·L < 0 => luz atrás da superfície => sem contribuição.
        // -------------------------------------------------------------
        float NdotL = dot(N, L);
        if (NdotL <= 0.0) continue;                      // superfície de costas para a luz

        vec3 diffuse = kd * NdotL;

        // -------------------------------------------------------------
        //  4.4 — ESPECULAR de Blinn-Phong
        //      H    = normalize(L + V)          (vetor meio-caminho)
        //      spec = max(N·H, 0)^α
        //  Vantagem sobre Phong: não precisa do vetor refletido R e não
        //  apresenta corte abrupto quando o ângulo R·V passa de 90°.
        //  Caso extremo: L = −V => L+V = 0 => H indefinido => spec = 0.
        // -------------------------------------------------------------
        vec3  halfSum = L + V;
        float spec    = 0.0;
        if (dot(halfSum, halfSum) > EPSILON) {
            vec3  H     = normalize(halfSum);
            float NdotH = max(dot(N, H), 0.0);
            spec        = pow(NdotH, alpha);
        }
        vec3 specular = vec3(ks * spec);                 // especular branco (cor da luz aplicada abaixo)

        // -------------------------------------------------------------
        //  4.5 — RADIÂNCIA DA LUZ i e ACUMULAÇÃO
        //      radiance = c · e · att · spot
        //      Lo += radiance · (diffuse + specular)
        // -------------------------------------------------------------
        vec3 radiance = light.colorIntensity.rgb * max(light.colorIntensity.a, 0.0)
                      * attenuation * spotFactor;

        Lo += radiance * (diffuse + specular);
    }

    // =================================================================
    //  PASSO 5 — TERMO AMBIENTE (luz indireta aproximada, constante)
    //      C_hdr = I_amb · kd + Lo
    // =================================================================
    vec3 colorHDR = lightData.ambientColor.rgb * kd + Lo;

    // Proteção: um único NaN viraria um pixel preto/"piscante".
    if (isInvalid(colorHDR)) {
        colorHDR = vec3(0.0);
    }

    // =================================================================
    //  PASSO 6 — MODOS DE DEPURAÇÃO (visualizar o G-Buffer)
    //  Útil para validar cada attachment isoladamente.
    // =================================================================
    if (pc.debugView != 0u) {
        vec3 dbg;
        if      (pc.debugView == 1u) dbg = fract(P * 0.1);          // posição (padrão repetido a cada 10 u)
        else if (pc.debugView == 2u) dbg = N * 0.5 + 0.5;          // normal [-1,1] -> [0,1]
        else if (pc.debugView == 3u) dbg = kd;                     // albedo
        else if (pc.debugView == 4u) dbg = vec3(ks);               // especular
        else {
            // Profundidade LINEAR (o depth é não linear em perspectiva):
            //   z_view = (n·f) / (f − depth·(f − n))
            float n = camera.nearPlane, f = camera.farPlane;
            float zLin = (n * f) / max(f - depth * (f - n), EPSILON);
            dbg = vec3(clamp((zLin - n) / (f - n), 0.0, 1.0));
        }
        outColor = vec4(dbg, 1.0);
        return;
    }

    // =================================================================
    //  PASSO 7 — EXPOSIÇÃO + TONE MAPPING + GAMA (cor final)
    //  A soma de várias luzes pode passar de 1.0 (HDR). Sem tone
    //  mapping, esses valores seriam cortados (áreas "estouradas").
    // =================================================================
    vec3 mapped = tonemapACES(colorHDR * max(pc.exposure, 0.0));

    // Correção gama: o monitor aplica ~x^2.2; compensamos com x^(1/2.2).
    // Se a swapchain for _SRGB, APPLY_GAMMA = 0 (o hardware faz isso).
    if (APPLY_GAMMA == 1u) {
        mapped = pow(mapped, vec3(1.0 / GAMMA));
    }

    outColor = vec4(mapped, 1.0);                        // RESULTADO FINAL DO PIXEL
}
