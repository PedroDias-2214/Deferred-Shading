#include "ObjLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

    struct Float3 { float x, y, z; };

    Float3 toF3(const float* p) { return { p[0], p[1], p[2] }; }
    Float3 sub(Float3 a, Float3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Float3 add(Float3 a, Float3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Float3 mul(Float3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    float dot(Float3 a, Float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Float3 cross(Float3 a, Float3 b) {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    Float3 normalized(Float3 a) {
        const float len2 = dot(a, a);
        return len2 > 1e-20f ? mul(a, 1.0f / std::sqrt(len2)) : Float3{ 0.0f, 0.0f, 0.0f };
    }

    // Converte índice do OBJ (base 1; negativo conta a partir do fim) para base 0.
    int resolveIndex(int idx, size_t count) {
        return idx > 0 ? idx - 1 : static_cast<int>(count) + idx;
    }

    // Um canto de face: índices de posição, UV e normal (-1 quando ausente).
    struct Corner {
        int v = -1;
        int t = -1;
        int n = -1;
    };

    // Aceita "v", "v/vt", "v//vn" e "v/vt/vn".
    Corner parseCorner(const std::string& token, size_t vCount, size_t tCount, size_t nCount) {
        Corner c;
        const size_t s1 = token.find('/');
        c.v = resolveIndex(std::stoi(token.substr(0, s1)), vCount);
        if (s1 == std::string::npos) return c;

        const size_t s2 = token.find('/', s1 + 1);
        const std::string vt = token.substr(s1 + 1,
            s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
        if (!vt.empty()) c.t = resolveIndex(std::stoi(vt), tCount);
        if (s2 != std::string::npos && s2 + 1 < token.size()) {
            c.n = resolveIndex(std::stoi(token.substr(s2 + 1)), nCount);
        }
        return c;
    }

} // namespace

MeshData ObjLoader::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Não foi possível abrir o modelo: " + path);
    }

    std::vector<Float3> positions;
    std::vector<Float3> normals;
    std::vector<float> uvs;  // pares (u, v)
    std::vector<std::array<Corner, 3>> triangles;

    // ---- Leitura linha a linha ----
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            Float3 p{};
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (tag == "vn") {
            Float3 n{};
            ss >> n.x >> n.y >> n.z;
            normals.push_back(normalized(n));
        }
        else if (tag == "vt") {
            float u = 0.0f, v = 0.0f;
            ss >> u >> v;
            uvs.push_back(u);
            uvs.push_back(v);
        }
        else if (tag == "f") {
            std::vector<Corner> corners;
            std::string tok;
            while (ss >> tok) {
                corners.push_back(parseCorner(tok, positions.size(), uvs.size() / 2, normals.size()));
            }
            // Polígonos com mais de três lados viram triângulos em leque.
            for (size_t i = 1; i + 1 < corners.size(); ++i) {
                triangles.push_back({ {corners[0], corners[i], corners[i + 1]} });
            }
        }
    }

    // ---- Vértices únicos e índices ----
    // Dois cantos com o mesmo (posição, UV, normal) compartilham o mesmo vértice.
    MeshData mesh;
    std::map<std::tuple<int, int, int, int>, uint32_t> unique;

    for (size_t ti = 0; ti < triangles.size(); ++ti) {
        const auto& tri = triangles[ti];

        // Normal da própria face: usada quando o arquivo não traz normais.
        const Float3 p0 = positions[tri[0].v];
        const Float3 p1 = positions[tri[1].v];
        const Float3 p2 = positions[tri[2].v];
        const Float3 faceNormal = normalized(cross(sub(p1, p0), sub(p2, p0)));

        for (const Corner& c : tri) {
            const bool hasNormal = c.n >= 0;
            const auto key = std::make_tuple(c.v, c.t, c.n, hasNormal ? -1 : static_cast<int>(ti));

            auto it = unique.find(key);
            if (it == unique.end()) {
                Vertex vert{};
                const Float3 p = positions[c.v];
                vert.position[0] = p.x;
                vert.position[1] = p.y;
                vert.position[2] = p.z;

                const Float3 n = hasNormal ? normals[c.n] : faceNormal;
                vert.normal[0] = n.x;
                vert.normal[1] = n.y;
                vert.normal[2] = n.z;

                if (c.t >= 0) {
                    vert.uv[0] = uvs[2 * c.t];
                    vert.uv[1] = uvs[2 * c.t + 1];
                }

                const uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(vert);
                it = unique.emplace(key, idx).first;
            }
            mesh.indices.push_back(it->second);
        }
    }

    if (mesh.vertices.empty()) {
        throw std::runtime_error("Modelo sem faces: " + path);
    }

    // ---- Tangentes (necessárias para o mapa de normal no geometry.vert) ----
    // Acumula, por triângulo, a direção do eixo U e do eixo V a partir de posições e UVs.
    std::vector<Float3> tAcc(mesh.vertices.size(), Float3{ 0.0f, 0.0f, 0.0f });
    std::vector<Float3> bAcc(mesh.vertices.size(), Float3{ 0.0f, 0.0f, 0.0f });

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t ia = mesh.indices[i];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];
        const Vertex& A = mesh.vertices[ia];
        const Vertex& B = mesh.vertices[ib];
        const Vertex& C = mesh.vertices[ic];

        const Float3 e1 = sub(toF3(B.position), toF3(A.position));
        const Float3 e2 = sub(toF3(C.position), toF3(A.position));
        const float du1 = B.uv[0] - A.uv[0], dv1 = B.uv[1] - A.uv[1];
        const float du2 = C.uv[0] - A.uv[0], dv2 = C.uv[1] - A.uv[1];
        const float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) continue;  // triângulo sem UV útil

        const float r = 1.0f / det;
        const Float3 t = mul(sub(mul(e1, dv2), mul(e2, dv1)), r);
        const Float3 b = mul(sub(mul(e2, du1), mul(e1, du2)), r);

        for (uint32_t idx : {ia, ib, ic}) {
            tAcc[idx] = add(tAcc[idx], t);
            bAcc[idx] = add(bAcc[idx], b);
        }
    }

    // Ortogonaliza a tangente contra a normal e guarda o sinal da bitangente em w.
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        Vertex& v = mesh.vertices[i];
        const Float3 n = toF3(v.normal);

        Float3 t = normalized(sub(tAcc[i], mul(n, dot(n, tAcc[i]))));
        if (dot(t, t) < 0.5f) {
            // Sem UV útil: qualquer direção perpendicular à normal serve.
            t = normalized(cross(n, std::fabs(n.x) < 0.9f ? Float3{ 1.0f, 0.0f, 0.0f }
            : Float3{ 0.0f, 1.0f, 0.0f }));
        }
        const float handedness = dot(cross(n, t), bAcc[i]) < 0.0f ? -1.0f : 1.0f;

        v.tangent[0] = t.x;
        v.tangent[1] = t.y;
        v.tangent[2] = t.z;
        v.tangent[3] = handedness;
    }

    // ---- Limites da malha (para centralizar e escalar na cena) ----
    for (int a = 0; a < 3; ++a) {
        mesh.boundsMin[a] = mesh.boundsMax[a] = mesh.vertices[0].position[a];
    }
    for (const Vertex& v : mesh.vertices) {
        for (int a = 0; a < 3; ++a) {
            mesh.boundsMin[a] = std::min(mesh.boundsMin[a], v.position[a]);
            mesh.boundsMax[a] = std::max(mesh.boundsMax[a], v.position[a]);
        }
    }

    return mesh;
}