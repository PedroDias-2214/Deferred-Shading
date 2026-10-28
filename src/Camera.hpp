#pragma once

#include "SceneData.hpp"

struct GLFWwindow;

// Câmera em primeira pessoa. Controles: W/A/S/D para andar, Q/E para descer e subir,
// mouse para olhar. Esc fecha a aplicação (tratado no App).
class Camera {
public:
    void init(GLFWwindow* window, uint32_t width, uint32_t height);
    void update(GLFWwindow* window, float dt);
    void fillCameraData(CameraData& out) const;

private:
    float m_pos[3] = { 0.0f, 0.0f, 3.0f };
    float m_yaw = -1.5707963f;   // -90 graus: olhando para -Z
    float m_pitch = 0.0f;
    float m_fovY = 1.0471976f;   // 60 graus
    float m_near = 0.1f;
    float m_far = 1000.0f;
    uint32_t m_width = 1;
    uint32_t m_height = 1;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_firstMouse = true;
};