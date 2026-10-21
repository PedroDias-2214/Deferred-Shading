// =====================================================================
//  fullscreen.vert  —  VERTEX SHADER do LIGHTING PASS (Subpass 1)
// ---------------------------------------------------------------------
//  Tema: Deferred Shading  |  Entrega 3 — Pseudocódigo dos Shaders
//
//  OBJETIVO
//    Gerar UM ÚNICO TRIÂNGULO que cobre a tela inteira, para que o
//    lighting.frag seja executado exatamente UMA vez por pixel.
//    É isso que dá ao Deferred Shading o custo O(pixels × luzes) em
//    vez de O(objetos × luzes) do Forward Rendering.
//
//  FLUXO DE DADOS
//    (nenhum Vertex Buffer!)  vkCmdDraw(cmd, 3, 1, 0, 0)
//    gl_VertexIndex ∈ {0,1,2}  ──►  [fullscreen.vert]
//    ──►  gl_Position (clip space) + vUV  ──►  lighting.frag
//
//  POR QUE UM TRIÂNGULO E NÃO UM QUADRADO (2 triângulos)?
//    Com 2 triângulos existe uma diagonal no meio da tela; os blocos
//    2x2 de pixels sobre essa diagonal são sombreados DUAS vezes
//    (helper invocations). O triângulo gigante evita esse desperdício;
//    a parte que sai da tela é removida pelo clipping, de graça.
// =====================================================================

#version 450

// ---------------------------------------------------------------------
//  ENTRADAS: nenhuma entrada declarada pelo usuário.
//  Usa apenas a variável embutida  gl_VertexIndex  (int, 0..2).
//  O pipeline é criado com vertexBindingDescriptionCount = 0.
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
//  SAÍDA para o fragment shader
// ---------------------------------------------------------------------
layout(location = 0) out vec2 vUV;   // coordenada de tela normalizada [0,1]² (0,0 = canto superior esquerdo)

void main() {
    // -----------------------------------------------------------------
    //  PASSO 1 — Gerar UV a partir do índice com operações de bits
    //
    //      índice  (i<<1)&2   i&2     UV
    //        0        0        0     (0,0)
    //        1        2        0     (2,0)
    //        2        0        2     (0,2)
    //
    //  O triângulo (0,0)-(2,0)-(0,2) em UV contém o quadrado [0,1]².
    // -----------------------------------------------------------------
    int i = gl_VertexIndex;
    vUV = vec2(float((i << 1) & 2), float(i & 2));

    // -----------------------------------------------------------------
    //  PASSO 2 — Converter UV [0,2] para NDC [-1,3]
    //      ndc = uv · 2 − 1
    //  No Vulkan, y = −1 é o TOPO da tela; logo uv.y = 0 (topo) já
    //  corresponde a ndc.y = −1, sem inversão manual.
    //  z = 0 (qualquer valor em [0,1] serve: o depth test está
    //  desativado neste subpass) e w = 1 (sem divisão perspectiva).
    // -----------------------------------------------------------------
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);

    //  Resultado após o clipping: a área visível do triângulo é
    //  exatamente o retângulo da tela; o rasterizador gera 1 fragmento
    //  por pixel e interpola vUV de 0 a 1 em cada eixo.
}
