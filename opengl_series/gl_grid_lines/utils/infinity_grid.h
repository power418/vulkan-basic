/*
    Blueprint: The Endless Grid
    (Refactored for C++11 and vk_grid_lines project)
*/

#pragma once

// Menggunakan header standar C++11
#include <cstdio>
#include <cstring>
#include <cmath>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

// Integrasi dengan library matematika kita sendiri
#include "math3d.h"

// --- Stub Konsep/Blueprint ---
// Karena kode asli bergantung pada "demolition_base_gl_app.h" yang tidak ada di proyek ini,
// kita mengubahnya menjadi blueprint mandiri yang nantinya bisa diintegrasikan 
// ke dalam arsitektur `RunImGuiApp` di main.cpp.

struct LightConcept {
    Vec3 WorldDirection;
    Vec3 WorldPosition;
    float DiffuseIntensity;
    float AmbientIntensity;
};

class EndlessGridBlueprint {
public:
    EndlessGridBlueprint() {
        // Setup awal lampu (Konsep)
        m_dirLight.WorldDirection = {0.0f, -1.0f, -1.0f};
        m_dirLight.DiffuseIntensity = 1.0f;

        m_pointLight.WorldPosition = {0.25f, 0.25f, 0.0f};
        m_pointLight.DiffuseIntensity = 2.0f;
        m_pointLight.AmbientIntensity = 0.1f;
    }

    ~EndlessGridBlueprint() {}

    void Start() {
        // Di sini nantinya inisialisasi Shader OpenGL untuk Infinite Grid sungguhan
        // m_pScene->GetConfig()->GetInfiniteGrid().Enabled = true;
        
        m_camera.pos = {0.0f, 0.75f, -2.5f};
        m_camera.pitch = -0.2f;
        m_camera.yaw = 0.0f;
    }

    // Fungsi ini dirancang agar cocok dengan AppRunner di main.cpp
    void UpdateAndRender(ImGuiIO& io, GLFWwindow* window) {
        m_count += 0.01f;

        // Simulasi pergerakan lampu
        m_pointLight.WorldPosition.x = std::sin(m_count);
        m_pointLight.WorldPosition.z = std::cos(m_count);

        // Input Keyboard (Tekan SPASI untuk memunculkan/menyembunyikan GUI)
        static bool spacePressed = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Space) && !spacePressed) {
            m_showGui = !m_showGui;
            spacePressed = true;
        } else if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
            spacePressed = false;
        }

        if (m_showGui) {
            RenderGUI();
        }

        // --- Render 3D Scene ---
        // (Nantinya di sini kita melakukan draw call OpenGL murni untuk Shader Infinite Grid)
    }

    float GetGridCellSize() const { return m_gridCellSize; }

private:
    void RenderGUI() {
        bool my_tool_active = true;

        // Tampilkan window kontrol Grid
        ImGui::Begin("Endless Grid Controls", &my_tool_active, ImGuiWindowFlags_MenuBar);      
        ImGui::SliderFloat("Grid Cell Size", &m_gridCellSize, 1.0f, 20.0f);
        
        // m_pScene->GetConfig()->GetInfiniteGrid().CellSize = m_gridCellSize;
        
        ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", m_camera.pos.x, m_camera.pos.y, m_camera.pos.z);
        ImGui::Text("Point Light: (%.2f, %.2f)", m_pointLight.WorldPosition.x, m_pointLight.WorldPosition.z);
        ImGui::End();
    }

    float m_count = 0.0f;
    float m_gridCellSize = 5.0f; // Ubah default menjadi 5.0f agar cocok dengan Viewport3D
    bool m_showGui = true;

    Camera3D m_camera;
    LightConcept m_dirLight;
    LightConcept m_pointLight;
};