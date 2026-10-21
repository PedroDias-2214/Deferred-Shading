// =====================================================================
//  geometry.vert  —  VERTEX SHADER do GEOMETRY PASS (Subpass 0)
// ---------------------------------------------------------------------
//  Tema: Deferred Shading  |  Entrega 3 — Pseudocódigo dos Shaders
//
//  OBJETIVO
//    Transformar cada vértice da malha do espaço do objeto (model space)
//    para o espaço de recorte (clip space) e preparar, para o Fragment
//    Shader do Geometry Pass, os dados geométricos em ESPAÇO DE MUNDO:
//    posição, base tangente (TBN) e coordenadas de textura.
//    NENHUM cálculo de iluminação é feito aqui — essa é a ideia central
//    do Deferred Shading: a geometria é processada uma única vez e a
//    luz só é calculada depois, no Lighting Pass.
//
//  FLUXO DE DADOS
//    Vertex Buffer (por vértice) + Push Constant (por objeto)
//    + UBO Câmera (por frame)  ──►  [geometry.vert]
//    ──►  gl_Position (rasterizador)  +  varyings  ──►  geometry.frag
// =====================================================================

#version 450                       // GLSL para Vulkan (SPIR-V)

// ---------------------------------------------------------------------
//  1. ENTRADAS POR VÉRTICE (Vertex Buffer, binding 0, VK_VERTEX_INPUT_RATE_VERTEX)
//     Cada atributo tem um "location" que corresponde ao
//     VkVertexInputAttributeDescription do pipeline.
// ---------------------------------------------------------------------
layout(location = 0) in vec3 inPosition;   // posição do vértice no espaço do OBJETO (x,y,z) — float32 x3
layout(location = 1) in vec3 inNormal;     // normal do vértice no espaço do objeto (pode não estar normalizada)
layout(location = 2) in vec2 inTexCoord;   // coordenada de textura UV, normalmente em [0,1] (pode passar de 1 com REPEAT)
layout(location = 3) in vec4 inTangent;    // tangente (xyz) + sinal da bitangente (w = +1 ou -1), padrão MikkTSpace/glTF

// ---------------------------------------------------------------------
//  2. UNIFORM BUFFER DA CÂMERA — Set 0, Binding 0 (mesmo UBO usado no Lighting Pass)
//     Atualizado uma vez por frame pela CPU. Layout std140.
// ---------------------------------------------------------------------
layout(set = 0, binding = 0, std140) uniform CameraUBO {
    mat4  view;          // matriz de visão  V : mundo  -> câmera
    mat4  proj;          // matriz de projeção P : câmera -> clip (já com o Y invertido do Vulkan)
    mat4  viewProj;      // P * V pré-multiplicada na CPU (economiza 1 mat4*mat4 por vértice)
    mat4  invView;       // V^-1 (usada no Lighting Pass)
    mat4  invProj;       // P^-1 (usada no Lighting Pass)
    vec4  cameraPos;     // posição da câmera no mundo (xyz); w não usado
    vec2  screenSize;    // resolução do framebuffer em pixels (largura, altura)
    float nearPlane;     // plano próximo  (ex.: 0.1)
    float farPlane;      // plano distante (ex.: 1000.0)
} camera;

// ---------------------------------------------------------------------
//  3. PUSH CONSTANTS — dados POR OBJETO (≤ 128 bytes garantidos pelo Vulkan)
//     Enviados com vkCmdPushConstants antes de cada vkCmdDrawIndexed.
// ---------------------------------------------------------------------
layout(push_constant) uniform ObjectPC {
    mat4 model;          // matriz de modelo M : objeto -> mundo (translação, rotação, escala)   64 bytes
    mat4 normalMatrix;   // (M^-1)^T calculada na CPU; só o bloco 3x3 é usado                    64 bytes
} object;                //                                                               total: 128 bytes

// ---------------------------------------------------------------------
//  4. SAÍDAS (varyings) para o geometry.frag
//     O rasterizador interpola esses valores de forma perspectivamente
//     correta entre os 3 vértices do triângulo (coordenadas baricêntricas).
// ---------------------------------------------------------------------
layout(location = 0) out vec3 vWorldPos;    // posição do vértice no MUNDO  (vai para o attachment POSITION)
layout(location = 1) out vec3 vNormal;      // normal no mundo  (N)
layout(location = 2) out vec3 vTangent;     // tangente no mundo (T)
layout(location = 3) out vec3 vBitangent;   // bitangente no mundo (B)
layout(location = 4) out vec2 vTexCoord;    // UV repassada sem alteração

// Constante auxiliar para evitar divisão por zero / vetores nulos.
const float EPSILON = 1e-6;

// ---------------------------------------------------------------------
//  Função auxiliar: normalização SEGURA.
//  normalize(v) = v / |v|. Se |v| ≈ 0 (malha com normal degenerada)
//  a divisão geraria NaN/Inf; nesse caso devolvemos um vetor padrão.
// ---------------------------------------------------------------------
vec3 safeNormalize(vec3 v, vec3 fallback) {
    float len2 = dot(v, v);                       // |v|² = x²+y²+z²  (mais barato que length)
    if (len2 < EPSILON) {                         // vetor nulo ou quase nulo
        return fallback;                          // evita NaN no G-Buffer
    }
    return v * inversesqrt(len2);                 // v * 1/sqrt(|v|²)
}

void main() {
    // -----------------------------------------------------------------
    //  PASSO 1 — Posição: objeto -> mundo
    //  Coordenadas homogêneas: w = 1 para PONTOS (a translação da matriz
    //  é aplicada).    P_mundo = M · [x y z 1]^T
    // -----------------------------------------------------------------
    vec4 worldPos4 = object.model * vec4(inPosition, 1.0);
    vWorldPos      = worldPos4.xyz / max(worldPos4.w, EPSILON);
    //   Obs.: para matrizes afins w já vale 1; a divisão protege
    //   contra matrizes de modelo não afins (caso extremo).

    // -----------------------------------------------------------------
    //  PASSO 2 — Base TBN no mundo
    //  NORMAL: usa a Normal Matrix (M^-1)^T, e não M, porque escalas NÃO
    //  UNIFORMES distorceriam a normal (ela deixaria de ser
    //  perpendicular à superfície).  w = 0 => vetores de direção não
    //  sofrem translação.
    // -----------------------------------------------------------------
    mat3 N3 = mat3(object.normalMatrix);          // extrai o bloco 3x3 superior-esquerdo
    mat3 M3 = mat3(object.model);

    vec3 N = safeNormalize(N3 * inNormal, vec3(0.0, 1.0, 0.0));

    // TANGENTE: segue a superfície, portanto é transformada por M
    // (e não pela normal matrix).
    vec3 T = safeNormalize(M3 * inTangent.xyz, vec3(1.0, 0.0, 0.0));

    // Ortogonalização de Gram-Schmidt: remove de T a componente paralela
    // a N, garantindo T ⟂ N mesmo após interpolação/escala.
    //      T' = normalize( T - (T·N) N )
    T = safeNormalize(T - dot(T, N) * N, vec3(1.0, 0.0, 0.0));

    // BITANGENTE: B = (N × T) · sinal. O sinal (inTangent.w) corrige UVs
    // espelhadas (mirrored UV), comuns em modelos simétricos.
    float handedness = (inTangent.w < 0.0) ? -1.0 : 1.0;   // qualquer valor inválido vira +1
    vec3  B = cross(N, T) * handedness;

    vNormal    = N;
    vTangent   = T;
    vBitangent = B;

    // -----------------------------------------------------------------
    //  PASSO 3 — Coordenada de textura: apenas repassada.
    //  O endereçamento (REPEAT/CLAMP) é tratado pelo sampler no .frag.
    // -----------------------------------------------------------------
    vTexCoord = inTexCoord;

    // -----------------------------------------------------------------
    //  PASSO 4 — Posição final em CLIP SPACE (saída obrigatória)
    //      gl_Position = P · V · M · p   =  viewProj · P_mundo
    //  Depois deste estágio o hardware faz: clipping, divisão
    //  perspectiva (x/w, y/w, z/w -> NDC), viewport transform e
    //  grava z_NDC ∈ [0,1] no DEPTH attachment (convenção Vulkan).
    // -----------------------------------------------------------------
    gl_Position = camera.viewProj * worldPos4;
}
