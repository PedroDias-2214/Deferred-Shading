#include "Camera.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstring>

namespace {

    struct Vec3 { float x, y, z; };

    Vec3 add(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vec3 scale(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vec3 cross(Vec3 a, Vec3 b) {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    Vec3 normalize(Vec3 a) { return scale(a, 1.0f / std::sqrt(dot(a, a))); }

    // Direção para onde a câmera olha, a partir dos ângulos yaw e pitch.
    Vec3 forwardFrom(float yaw, float pitch) {
        return { std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw) };
    }

    // Multiplicação de matrizes 4x4 em ordem de colunas: r = a * b.
    void mul(const float* a, const float* b, float* r) {
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    s += a[k * 4 + row] * b[c * 4 + k];
                }
                r[c * 4 + row] = s;
            }
        }
    }

} // namespace

void Camera::init(GLFWwindow* window, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    // Cursor preso na janela: o mouse controla só o olhar.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Camera::update(GLFWwindow* window, float dt) {
    // Olhar: a diferença de posição do mouse entre quadros vira ângulo.
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    if (m_firstMouse) {
        m_lastX = x;
        m_lastY = y;
        m_firstMouse = false;
    }
    const float sensitivity = 0.0025f;  // radianos por pixel
    m_yaw += static_cast<float>(x - m_lastX) * sensitivity;
    m_pitch -= static_cast<float>(y - m_lastY) * sensitivity;
    m_lastX = x;
    m_lastY = y;

    // Limita o pitch para a câmera não virar de cabeça para baixo.
    const float limit = 1.55f;
    if (m_pitch > limit) m_pitch = limit;
    if (m_pitch < -limit) m_pitch = -limit;

    // Andar: relativo à direção em que a câmera olha.
    const Vec3 f = forwardFrom(m_yaw, m_pitch);
    const Vec3 r = normalize(cross(f, { 0.0f, 1.0f, 0.0f }));
    const Vec3 up = { 0.0f, 1.0f, 0.0f };

    Vec3 move = { 0.0f, 0.0f, 0.0f };
    auto pressed = [window](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
    if (pressed(GLFW_KEY_W)) move = add(move, f);
    if (pressed(GLFW_KEY_S)) move = add(move, scale(f, -1.0f));
    if (pressed(GLFW_KEY_D)) move = add(move, r);
    if (pressed(GLFW_KEY_A)) move = add(move, scale(r, -1.0f));
    if (pressed(GLFW_KEY_E)) move = add(move, up);
    if (pressed(GLFW_KEY_Q)) move = add(move, scale(up, -1.0f));

    if (dot(move, move) > 0.0f) {
        const float speed = 3.0f;  // unidades por segundo
        const Vec3 step = scale(normalize(move), speed * dt);
        m_pos[0] += step.x;
        m_pos[1] += step.y;
        m_pos[2] += step.z;
    }
}

void Camera::fillCameraData(CameraData& out) const {
    const Vec3 eye = { m_pos[0], m_pos[1], m_pos[2] };
    const Vec3 f = forwardFrom(m_yaw, m_pitch);
    const Vec3 r = normalize(cross(f, { 0.0f, 1.0f, 0.0f }));
    const Vec3 u = cross(r, f);

    // Visão (mundo -> câmera), em ordem de colunas.
    const float view[16] = {
        r.x,          u.x,          -f.x,         0.0f,
        r.y,          u.y,          -f.y,         0.0f,
        r.z,          u.z,          -f.z,         0.0f,
        -dot(r, eye), -dot(u, eye), dot(f, eye),  1.0f,
    };

    // Projeção perspectiva do Vulkan: profundidade em [0, 1] e eixo Y invertido.
    const float fy = 1.0f / std::tan(m_fovY * 0.5f);
    const float a = fy / (static_cast<float>(m_width) / static_cast<float>(m_height));
    const float b = -fy;
    const float c = -m_far / (m_far - m_near);
    const float d = -(m_far * m_near) / (m_far - m_near);
    const float proj[16] = {
        a,    0.0f, 0.0f, 0.0f,
        0.0f, b,    0.0f, 0.0f,
        0.0f, 0.0f, c,    -1.0f,
        0.0f, 0.0f, d,    0.0f,
    };

    // Inversa da visão: a pose da câmera (colunas = direita, cima, trás e posição).
    const float invView[16] = {
        r.x,   r.y,   r.z,   0.0f,
        u.x,   u.y,   u.z,   0.0f,
        -f.x,  -f.y,  -f.z,  0.0f,
        eye.x, eye.y, eye.z, 1.0f,
    };

    // Inversa da projeção, com os termos recalculados a partir de a, b, c e d.
    const float invProj[16] = {
        1.0f / a, 0.0f,     0.0f, 0.0f,
        0.0f,     1.0f / b, 0.0f, 0.0f,
        0.0f,     0.0f,     0.0f, 1.0f / d,
        0.0f,     0.0f,     -1.0f, c / d,
    };

    std::memcpy(out.view, view, sizeof(view));
    std::memcpy(out.proj, proj, sizeof(proj));
    mul(proj, view, out.viewProj);
    std::memcpy(out.invView, invView, sizeof(invView));
    std::memcpy(out.invProj, invProj, sizeof(invProj));

    out.cameraPos[0] = eye.x;
    out.cameraPos[1] = eye.y;
    out.cameraPos[2] = eye.z;
    out.cameraPos[3] = 1.0f;
    out.screenSize[0] = static_cast<float>(m_width);
    out.screenSize[1] = static_cast<float>(m_height);
    out.nearPlane = m_near;
    out.farPlane = m_far;
}