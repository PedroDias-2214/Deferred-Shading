#include "GltfLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <numeric>
#include <stdexcept>

// tinygltf decodifica as imagens embutidas com o stb_image. As definições abaixo
// ativam o código de cada biblioteca nesta única unidade de compilação.
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace {

    using Mat4 = std::array<float, 16>;  // ordem de colunas, igual ao GLSL

    struct Float3 { float x, y, z; };

    Float3 f3(const float* p) { return { p[0], p[1], p[2] }; }
    Float3 add(Float3 a, Float3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Float3 sub(Float3 a, Float3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Float3 mul(Float3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    float dot(Float3 a, Float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Float3 cross(Float3 a, Float3 b) {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    Float3 normalized(Float3 a) {
        const float len2 = dot(a, a);
        return len2 > 1e-20f ? mul(a, 1.0f / std::sqrt(len2)) : Float3{ 0.0f, 0.0f, 0.0f };
    }

    Mat4 identity() {
        Mat4 m{};
        m[0] = m[5] = m[10] = m[15] = 1.0f;
        return m;
    }

    // r = a * b, em ordem de colunas.
    Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += a[k * 4 + row] * b[c * 4 + k];
                r[c * 4 + row] = s;
            }
        }
        return r;
    }

    // Matriz local do nó: usa a matriz direta se existir; senão compõe T * R * S.
    Mat4 localMatrix(const tinygltf::Node& node) {
        if (node.matrix.size() == 16) {
            Mat4 m{};
            for (int i = 0; i < 16; ++i) m[i] = static_cast<float>(node.matrix[i]);
            return m;
        }

        Mat4 t = identity();
        if (node.translation.size() == 3) {
            t[12] = static_cast<float>(node.translation[0]);
            t[13] = static_cast<float>(node.translation[1]);
            t[14] = static_cast<float>(node.translation[2]);
        }

        Mat4 r = identity();
        if (node.rotation.size() == 4) {
            const float x = static_cast<float>(node.rotation[0]);
            const float y = static_cast<float>(node.rotation[1]);
            const float z = static_cast<float>(node.rotation[2]);
            const float w = static_cast<float>(node.rotation[3]);
            r[0] = 1 - 2 * (y * y + z * z);
            r[1] = 2 * (x * y + z * w);
            r[2] = 2 * (x * z - y * w);
            r[4] = 2 * (x * y - z * w);
            r[5] = 1 - 2 * (x * x + z * z);
            r[6] = 2 * (y * z + x * w);
            r[8] = 2 * (x * z + y * w);
            r[9] = 2 * (y * z - x * w);
            r[10] = 1 - 2 * (x * x + y * y);
        }

        Mat4 s = identity();
        if (node.scale.size() == 3) {
            s[0] = static_cast<float>(node.scale[0]);
            s[5] = static_cast<float>(node.scale[1]);
            s[10] = static_cast<float>(node.scale[2]);
        }

        return multiply(t, multiply(r, s));
    }

    Float3 transformPoint(const Mat4& m, Float3 p) {
        return { m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14] };
    }

    // Normal transformada por (A^-1)^T, onde A é a parte 3x3 da matriz.
    // (A^-1)^T é a matriz de cofatores C dividida pelo determinante.
    Float3 transformNormal(const Mat4& m, Float3 n) {
        const float a00 = m[0], a01 = m[4], a02 = m[8];
        const float a10 = m[1], a11 = m[5], a12 = m[9];
        const float a20 = m[2], a21 = m[6], a22 = m[10];

        const float c00 = a11 * a22 - a12 * a21;
        const float c01 = -(a10 * a22 - a12 * a20);
        const float c02 = a10 * a21 - a11 * a20;
        const float c10 = -(a01 * a22 - a02 * a21);
        const float c11 = a00 * a22 - a02 * a20;
        const float c12 = -(a00 * a21 - a01 * a20);
        const float c20 = a01 * a12 - a02 * a11;
        const float c21 = -(a00 * a12 - a02 * a10);
        const float c22 = a00 * a11 - a01 * a10;

        const float det = a00 * c00 + a01 * c01 + a02 * c02;
        if (std::fabs(det) < 1e-12f) return normalized(n);

        const float inv = 1.0f / det;
        return normalized({ (c00 * n.x + c01 * n.y + c02 * n.z) * inv,
                           (c10 * n.x + c11 * n.y + c12 * n.z) * inv,
                           (c20 * n.x + c21 * n.y + c22 * n.z) * inv });
    }

    // Lê um accessor do tipo FLOAT como lista plana (numComp valores por elemento).
    std::vector<float> readFloats(const tinygltf::Model& gltf, int accessorIndex, int numComp) {
        const tinygltf::Accessor& acc = gltf.accessors.at(accessorIndex);
        if (acc.bufferView < 0) throw std::runtime_error("Accessor sem bufferView");
        if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            throw std::runtime_error("Atributo glTF não é FLOAT");
        }

        const tinygltf::BufferView& view = gltf.bufferViews.at(acc.bufferView);
        const tinygltf::Buffer& buffer = gltf.buffers.at(view.buffer);
        const size_t elemSize = sizeof(float) * numComp;
        const size_t stride = view.byteStride > 0 ? static_cast<size_t>(view.byteStride) : elemSize;
        const unsigned char* base = buffer.data.data() + view.byteOffset + acc.byteOffset;

        std::vector<float> out(acc.count * numComp);
        for (size_t i = 0; i < acc.count; ++i) {
            std::memcpy(&out[i * numComp], base + i * stride, elemSize);
        }
        return out;
    }

    // Lê os índices de um accessor, aceitando 8, 16 e 32 bits.
    std::vector<uint32_t> readIndices(const tinygltf::Model& gltf, int accessorIndex) {
        const tinygltf::Accessor& acc = gltf.accessors.at(accessorIndex);
        if (acc.bufferView < 0) throw std::runtime_error("Accessor de índices sem bufferView");

        const tinygltf::BufferView& view = gltf.bufferViews.at(acc.bufferView);
        const tinygltf::Buffer& buffer = gltf.buffers.at(view.buffer);
        const unsigned char* base = buffer.data.data() + view.byteOffset + acc.byteOffset;

        size_t size = 0;
        switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  size = 1; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: size = 2; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   size = 4; break;
        default: throw std::runtime_error("Tipo de índice glTF não suportado");
        }
        const size_t stride = view.byteStride > 0 ? static_cast<size_t>(view.byteStride) : size;

        std::vector<uint32_t> out(acc.count);
        for (size_t i = 0; i < acc.count; ++i) {
            const unsigned char* p = base + i * stride;
            switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out[i] = *p;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                uint16_t v = 0;
                std::memcpy(&v, p, 2);
                out[i] = v;
                break;
            }
            default: {
                uint32_t v = 0;
                std::memcpy(&v, p, 4);
                out[i] = v;
                break;
            }
            }
        }
        return out;
    }

    // Converte a imagem decodificada pelo tinygltf para RGBA de 8 bits.
    ImageRGBA toRGBA(const tinygltf::Image& img) {
        if (img.bits != 8 || img.image.empty()) {
            throw std::runtime_error("Imagem glTF não decodificada (esperado 8 bits por canal)");
        }

        ImageRGBA out;
        out.width = static_cast<uint32_t>(img.width);
        out.height = static_cast<uint32_t>(img.height);
        const size_t count = static_cast<size_t>(out.width) * out.height;
        out.pixels.resize(count * 4);

        const int c = img.component;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* s = &img.image[i * c];
            uint8_t* d = &out.pixels[i * 4];
            switch (c) {
            case 1: d[0] = d[1] = d[2] = s[0]; d[3] = 255; break;
            case 2: d[0] = d[1] = d[2] = s[0]; d[3] = s[1]; break;
            case 3: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; break;
            default: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; break;
            }
        }
        return out;
    }

    ImageRGBA loadTexture(const tinygltf::Model& gltf, int textureIndex) {
        const tinygltf::Texture& tex = gltf.textures.at(textureIndex);
        if (tex.source < 0) throw std::runtime_error("Textura glTF sem imagem");
        return toRGBA(gltf.images.at(tex.source));
    }

    // Junta as primitivas de um nó (já com a transformação do nó) na malha de saída.
    void appendPrimitive(const tinygltf::Model& gltf, const tinygltf::Primitive& prim,
        const Mat4& world, MeshData& mesh) {
        if (prim.mode != TINYGLTF_MODE_TRIANGLES) return;

        const auto pos = prim.attributes.find("POSITION");
        const auto nrm = prim.attributes.find("NORMAL");
        const auto uv = prim.attributes.find("TEXCOORD_0");
        if (pos == prim.attributes.end()) return;
        if (nrm == prim.attributes.end()) throw std::runtime_error("Modelo glTF sem normais");

        const std::vector<float> positions = readFloats(gltf, pos->second, 3);
        const std::vector<float> normals = readFloats(gltf, nrm->second, 3);
        const size_t count = positions.size() / 3;
        const std::vector<float> uvs = uv != prim.attributes.end()
            ? readFloats(gltf, uv->second, 2)
            : std::vector<float>(count * 2, 0.0f);

        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (size_t i = 0; i < count; ++i) {
            const Float3 p = transformPoint(world, f3(&positions[i * 3]));
            const Float3 n = transformNormal(world, f3(&normals[i * 3]));

            Vertex v{};
            v.position[0] = p.x; v.position[1] = p.y; v.position[2] = p.z;
            v.normal[0] = n.x; v.normal[1] = n.y; v.normal[2] = n.z;
            v.uv[0] = uvs[i * 2];
            v.uv[1] = uvs[i * 2 + 1];
            mesh.vertices.push_back(v);
        }

        std::vector<uint32_t> indices;
        if (prim.indices >= 0) {
            indices = readIndices(gltf, prim.indices);
        }
        else {
            indices.resize(count);
            std::iota(indices.begin(), indices.end(), 0u);
        }
        for (uint32_t i : indices) {
            mesh.indices.push_back(base + i);
        }
    }

    // Tangentes a partir das UVs. São necessárias para o mapa de normal no geometry.frag.
    void computeTangents(MeshData& mesh) {
        const size_t n = mesh.vertices.size();
        std::vector<Float3> tAcc(n, Float3{ 0.0f, 0.0f, 0.0f });
        std::vector<Float3> bAcc(n, Float3{ 0.0f, 0.0f, 0.0f });

        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const uint32_t ia = mesh.indices[i];
            const uint32_t ib = mesh.indices[i + 1];
            const uint32_t ic = mesh.indices[i + 2];
            const Vertex& A = mesh.vertices[ia];
            const Vertex& B = mesh.vertices[ib];
            const Vertex& C = mesh.vertices[ic];

            const Float3 e1 = sub(f3(B.position), f3(A.position));
            const Float3 e2 = sub(f3(C.position), f3(A.position));
            const float du1 = B.uv[0] - A.uv[0], dv1 = B.uv[1] - A.uv[1];
            const float du2 = C.uv[0] - A.uv[0], dv2 = C.uv[1] - A.uv[1];
            const float det = du1 * dv2 - du2 * dv1;
            if (std::fabs(det) < 1e-12f) continue;

            const float r = 1.0f / det;
            const Float3 t = mul(sub(mul(e1, dv2), mul(e2, dv1)), r);
            const Float3 b = mul(sub(mul(e2, du1), mul(e1, du2)), r);

            for (uint32_t idx : {ia, ib, ic}) {
                tAcc[idx] = add(tAcc[idx], t);
                bAcc[idx] = add(bAcc[idx], b);
            }
        }

        for (size_t i = 0; i < n; ++i) {
            Vertex& v = mesh.vertices[i];
            const Float3 nrm = f3(v.normal);

            Float3 t = normalized(sub(tAcc[i], mul(nrm, dot(nrm, tAcc[i]))));
            if (dot(t, t) < 0.5f) {
                t = normalized(cross(nrm, std::fabs(nrm.x) < 0.9f ? Float3{ 1.0f, 0.0f, 0.0f }
                : Float3{ 0.0f, 1.0f, 0.0f }));
            }
            const float handedness = dot(cross(nrm, t), bAcc[i]) < 0.0f ? -1.0f : 1.0f;

            v.tangent[0] = t.x;
            v.tangent[1] = t.y;
            v.tangent[2] = t.z;
            v.tangent[3] = handedness;
        }
    }

    void computeBounds(MeshData& mesh) {
        for (int a = 0; a < 3; ++a) {
            mesh.boundsMin[a] = mesh.boundsMax[a] = mesh.vertices[0].position[a];
        }
        for (const Vertex& v : mesh.vertices) {
            for (int a = 0; a < 3; ++a) {
                mesh.boundsMin[a] = std::min(mesh.boundsMin[a], v.position[a]);
                mesh.boundsMax[a] = std::max(mesh.boundsMax[a], v.position[a]);
            }
        }
    }

} // namespace

ModelData GltfLoader::load(const std::string& path) {
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    const bool isBinary = path.size() >= 4 && path.compare(path.size() - 4, 4, ".glb") == 0;
    const bool ok = isBinary
        ? loader.LoadBinaryFromFile(&gltf, &err, &warn, path)
        : loader.LoadASCIIFromFile(&gltf, &err, &warn, path);
    if (!warn.empty()) std::fprintf(stderr, "[glTF] aviso: %s\n", warn.c_str());
    if (!ok) throw std::runtime_error("Falha ao ler " + path + ": " + err);
    if (gltf.scenes.empty()) throw std::runtime_error("Arquivo glTF sem cena");

    ModelData out;

    // Material: usa o primeiro material encontrado nas primitivas.
    int materialIndex = -1;
    for (const auto& mesh : gltf.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (materialIndex < 0 && prim.material >= 0) materialIndex = prim.material;
        }
    }
    if (materialIndex >= 0) {
        const tinygltf::Material& mat = gltf.materials.at(materialIndex);
        const auto& factor = mat.pbrMetallicRoughness.baseColorFactor;
        for (size_t i = 0; i < 4 && i < factor.size(); ++i) {
            out.baseColorFactor[i] = static_cast<float>(factor[i]);
        }
        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            out.albedo = loadTexture(gltf, mat.pbrMetallicRoughness.baseColorTexture.index);
        }
        if (mat.normalTexture.index >= 0) {
            out.normal = loadTexture(gltf, mat.normalTexture.index);
        }
    }

    // Geometria: percorre a árvore de nós acumulando as transformações.
    const int sceneIndex = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
    const tinygltf::Scene& scene = gltf.scenes.at(sceneIndex);

    std::function<void(int, const Mat4&)> visit = [&](int nodeIndex, const Mat4& parent) {
        const tinygltf::Node& node = gltf.nodes.at(nodeIndex);
        const Mat4 world = multiply(parent, localMatrix(node));
        if (node.mesh >= 0) {
            for (const auto& prim : gltf.meshes.at(node.mesh).primitives) {
                appendPrimitive(gltf, prim, world, out.mesh);
            }
        }
        for (int child : node.children) visit(child, world);
        };
    for (int root : scene.nodes) visit(root, identity());

    if (out.mesh.indices.empty()) throw std::runtime_error("Modelo sem triângulos: " + path);

    computeTangents(out.mesh);
    computeBounds(out.mesh);
    return out;
}