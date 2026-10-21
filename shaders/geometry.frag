// =====================================================================
//  geometry.frag  —  FRAGMENT SHADER do GEOMETRY PASS (Subpass 0)
// ---------------------------------------------------------------------
//  Tema: Deferred Shading  |  Entrega 3 — Pseudocódigo dos Shaders
//
//  OBJETIVO
//    Para cada fragmento visível da cena, amostrar as texturas do
//    material e GRAVAR os atributos de superfície no G-Buffer através
//    de MRT (Multiple Render Targets). Este shader NÃO calcula luz:
//    ele apenas "fotografa" a geometria em buffers intermediários.
//
//  FLUXO DE DADOS
//    varyings do geometry.vert (interpolados pelo rasterizador)
//    + texturas do material (Set 1)
//    ──► [geometry.frag] ──►  G-Buffer:
//         location 0  POSITION    (RGBA16F)  = posição no mundo + marcador
//         location 1  NORMAL      (RGBA16F)  = normal no mundo + brilho
//         location 2  ALBEDO_SPEC (RGBA8)    = cor base + intensidade especular
//         DEPTH       (D32_SFLOAT)           = escrito pelo hardware (depth test LESS)
//
//  O Early-Z (teste de profundidade antes do fragment shader) descarta
//  fragmentos ocultos; assim, idealmente, só o fragmento mais próximo
//  de cada pixel chega a escrever no G-Buffer.
// =====================================================================

#version 450

// ---------------------------------------------------------------------
//  1. ENTRADAS — mesmas locations das saídas do geometry.vert
// ---------------------------------------------------------------------
layout(location = 0) in vec3 vWorldPos;    // posição no mundo, interpolada
layout(location = 1) in vec3 vNormal;      // normal no mundo (a interpolação DESNORMALIZA o vetor)
layout(location = 2) in vec3 vTangent;     // tangente no mundo
layout(location = 3) in vec3 vBitangent;   // bitangente no mundo
layout(location = 4) in vec2 vTexCoord;    // coordenada UV

// ---------------------------------------------------------------------
//  2. RECURSOS DO MATERIAL — Descriptor Set 1 (trocado por material)
//
//  Tipo: sampler2D = COMBINED_IMAGE_SAMPLER (imagem + sampler juntos).
//  Configuração do VkSampler usado nas três texturas:
//     magFilter  = VK_FILTER_LINEAR                (ampliação: bilinear)
//     minFilter  = VK_FILTER_LINEAR                (redução:  bilinear)
//     mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR   (interpola entre 2 mips = TRILINEAR)
//     addressU/V = VK_SAMPLER_ADDRESS_MODE_REPEAT  (UV fora de [0,1] repete o padrão)
//     anisotropyEnable = VK_TRUE, maxAnisotropy = 16 (nitidez em ângulos rasantes)
//     minLod = 0, maxLod = VK_LOD_CLAMP_NONE       (todos os níveis de mipmap disponíveis)
//
//  Formatos:
//     albedoMap   -> VK_FORMAT_R8G8B8A8_SRGB  (hardware converte sRGB -> linear na leitura)
//     normalMap   -> VK_FORMAT_R8G8B8A8_UNORM (dados, NÃO é cor: sem conversão sRGB)
//     specularMap -> VK_FORMAT_R8_UNORM       (1 canal: intensidade especular)
// ---------------------------------------------------------------------
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D specularMap;

// Parâmetros escalares do material (UBO std140, Set 1, binding 3)
layout(set = 1, binding = 3, std140) uniform MaterialUBO {
    vec4  baseColorFactor;   // multiplicador RGBA da cor base (1,1,1,1 = sem alteração)
    float specularFactor;    // multiplicador da intensidade especular  [0,1]
    float shininess;         // expoente de brilho de Blinn-Phong        [1, 256]
    float alphaCutoff;       // limiar do alpha test (ex.: 0.5) — folhas, grades
    uint  flags;             // bit 0: possui normal map | bit 1: possui specular map | bit 2: alpha test
} material;

const uint  FLAG_HAS_NORMAL_MAP   = 1u;
const uint  FLAG_HAS_SPECULAR_MAP = 2u;
const uint  FLAG_ALPHA_TEST       = 4u;
const float MAX_SHININESS         = 256.0;   // usado para normalizar o brilho em [0,1]
const float EPSILON               = 1e-6;

// ---------------------------------------------------------------------
//  3. SAÍDAS — attachments de cor do G-Buffer (MRT)
//     A "location" é o índice em pColorAttachments do Subpass 0.
// ---------------------------------------------------------------------
layout(location = 0) out vec4 outPosition;    // .xyz = posição no mundo | .w = 1.0 (marca "há geometria")
layout(location = 1) out vec4 outNormal;      // .xyz = normal no mundo normalizada | .w = shininess / 256
layout(location = 2) out vec4 outAlbedoSpec;  // .rgb = albedo linear | .a = intensidade especular

vec3 safeNormalize(vec3 v, vec3 fallback) {
    float len2 = dot(v, v);
    return (len2 < EPSILON) ? fallback : v * inversesqrt(len2);
}

void main() {
    // =================================================================
    //  PASSO 1 — AMOSTRAGEM DO ALBEDO (cor base)
    //
    //  texture() escolhe o NÍVEL DE MIPMAP automaticamente, usando as
    //  derivadas de tela da UV (calculadas em blocos de 2x2 pixels):
    //
    //      dx = dFdx(uv) * texSize ,   dy = dFdy(uv) * texSize
    //      ρ  = max( |dx| , |dy| )                (texels por pixel)
    //      λ  = log2(ρ)                           (nível de mip, LOD)
    //      λ  = clamp(λ + mipLodBias, minLod, maxLod)
    //
    //  Com mipmapMode LINEAR, o resultado é a mistura dos níveis
    //  floor(λ) e floor(λ)+1 com peso fract(λ), cada um filtrado
    //  bilinearmente (4 texels) => filtragem TRILINEAR (8 texels).
    //  A anisotropia soma até 16 amostras ao longo do eixo maior.
    //  Isso evita aliasing/"cintilação" em superfícies distantes.
    // =================================================================
    vec4 albedoSample = texture(albedoMap, vTexCoord);
    vec4 baseColor    = albedoSample * material.baseColorFactor;

    // -----------------------------------------------------------------
    //  PASSO 2 — ALPHA TEST (condição de descarte)
    //  O Deferred Shading não suporta transparência por mistura (há um
    //  único valor por pixel no G-Buffer). Materiais recortados
    //  (folhagem, cercas) usam alpha test: se alpha < limiar, o
    //  fragmento é DESCARTADO e não escreve em nenhum attachment nem
    //  no depth. Objetos translúcidos de verdade são desenhados num
    //  passe forward separado (fora do escopo desta entrega).
    // -----------------------------------------------------------------
    if ((material.flags & FLAG_ALPHA_TEST) != 0u && baseColor.a < material.alphaCutoff) {
        discard;
    }

    // =================================================================
    //  PASSO 3 — NORMAL DA SUPERFÍCIE (com ou sem normal mapping)
    //
    //  A interpolação linear entre vértices encurta os vetores, então
    //  RE-NORMALIZAMOS N, T e B no fragmento.
    // =================================================================
    vec3 N = safeNormalize(vNormal,    vec3(0.0, 1.0, 0.0));
    vec3 T = safeNormalize(vTangent,   vec3(1.0, 0.0, 0.0));
    vec3 B = safeNormalize(vBitangent, vec3(0.0, 0.0, 1.0));

    // Triângulo visto pelas costas (materiais "double-sided"):
    // gl_FrontFacing = false => inverte a normal para que ela aponte
    // para a câmera; caso contrário a face ficaria preta no Lighting Pass.
    if (!gl_FrontFacing) {
        N = -N;  T = -T;  B = -B;
    }

    if ((material.flags & FLAG_HAS_NORMAL_MAP) != 0u) {
        // Normal map guarda a normal no ESPAÇO TANGENTE codificada em
        // [0,1]. Decodificação para [-1,1]:   n_t = 2·rgb − 1
        vec3 nTangent = texture(normalMap, vTexCoord).rgb * 2.0 - 1.0;

        // Reconstrução de Z (robustez para mapas comprimidos BC5, que
        // guardam apenas X e Y):  z = sqrt( max(0, 1 − x² − y²) )
        nTangent.z = sqrt(max(0.0, 1.0 - dot(nTangent.xy, nTangent.xy)));

        // Matriz TBN (colunas T, B, N) leva do espaço tangente ao mundo:
        //      n_mundo = TBN · n_t = T·x + B·y + N·z
        mat3 TBN = mat3(T, B, N);
        N = safeNormalize(TBN * nTangent, N);   // se der vetor nulo, mantém a normal geométrica
    }

    // =================================================================
    //  PASSO 4 — INTENSIDADE ESPECULAR
    //  Mapa de 1 canal (R). Sem mapa, usa apenas o fator do material.
    // =================================================================
    float specIntensity = material.specularFactor;
    if ((material.flags & FLAG_HAS_SPECULAR_MAP) != 0u) {
        specIntensity *= texture(specularMap, vTexCoord).r;
    }
    specIntensity = clamp(specIntensity, 0.0, 1.0);   // RGBA8 só armazena [0,1]

    // Brilho (expoente de Blinn-Phong) normalizado para caber em [0,1].
    float shininessNorm = clamp(material.shininess, 1.0, MAX_SHININESS) / MAX_SHININESS;

    // =================================================================
    //  PASSO 5 — ESCRITA NO G-BUFFER (resultado final deste shader)
    //  Cada location vai para um attachment diferente, em paralelo.
    // =================================================================
    outPosition   = vec4(vWorldPos, 1.0);          // w = 1 => pixel COM geometria (o clear é 0)
    outNormal     = vec4(N, shininessNorm);        // formato float: guarda [-1,1] sem codificação
    outAlbedoSpec = vec4(clamp(baseColor.rgb, 0.0, 1.0), specIntensity);

    // A PROFUNDIDADE não é escrita manualmente: o hardware grava
    // gl_FragCoord.z no DEPTH attachment (depthWriteEnable = VK_TRUE,
    // depthCompareOp = VK_COMPARE_OP_LESS). Não escrever gl_FragDepth
    // mantém o Early-Z ativo.
}
