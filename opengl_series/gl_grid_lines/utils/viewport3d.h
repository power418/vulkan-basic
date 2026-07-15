#pragma once
#include <cmath>
#include <string>

#include <imgui.h>

#include "math3d.h"

#ifdef _WIN32
#include <GL/GL.h>
#endif

// Kelas Viewport3D membungkus seluruh kerumitan:
// 1. Inisialisasi Jendela ImGui (Windowing)
// 2. Input Kamera (WASD & Mouselook)
// 3. Proyeksi 3D ke 2D (Renderer)
// 4. Raycasting (Pointing)
class Viewport3D {
public:
    Camera3D camera;
    ImVec2 p0;
    ImVec2 sz;
    ImDrawList* draw_list;
    ImGuiIO* io;

    Viewport3D() {}

    // Membuka jendela rendering ImGui & menangani input kamera
    void BeginRender(const std::string& window_name, ImGuiIO& current_io) {
        io = &current_io;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io->DisplaySize, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(window_name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

        p0 = ImGui::GetCursorScreenPos();
        sz = ImGui::GetContentRegionAvail();
        draw_list = ImGui::GetWindowDrawList();

        HandleInput();
    }

    // Menutup jendela rendering ImGui
    void EndRender() {
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // Menggambar lantai grid 3D (Infinite Grid) lengkap dengan sumbu warna
    // (Pindah ke render_opengl di main.cpp)

    // Fungsi Raycast (Kini menggunakan Klik Kiri)
    bool RaycastToGround(Vec3& out_hit_pos, bool click_only = true) {
        bool is_active = click_only ? ImGui::IsMouseClicked(ImGuiMouseButton_Left) : ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (ImGui::IsWindowHovered() && is_active) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float scl = 500.0f * camera.fovY;
            
            float ndc_x = (mouse_pos.x - (p0.x + sz.x * 0.5f)) / scl;
            float ndc_y = -(mouse_pos.y - (p0.y + sz.y * 0.5f)) / scl;

            Vec3 forward = camera.GetForward();
            Vec3 right = camera.GetRight();
            Vec3 up = Cross(right, forward);

            Vec3 ray_dir = right * ndc_x + up * ndc_y + forward;

            if (std::abs(ray_dir.y) > 1e-5f) {
                float t = -camera.pos.y / ray_dir.y;
                if (t > 0.0f) {
                    out_hit_pos = camera.pos + ray_dir * t;
                    return true;
                }
            }
        }
        return false;
    }

    // === NATIVE OPENGL RENDERING ===
    
    void SetupOpenGLMatrices(float display_w, float display_h) {
#ifdef _WIN32
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        
        float scl = 500.0f * camera.fovY;
        float zNear = 0.1f;
        float zFar = 2000.0f;
        float right = (display_w * zNear) / (2.0f * scl);
        float top = (display_h * zNear) / (2.0f * scl);
        
        // Asumsikan kita memakai gluPerspective/glFrustum logic
        // Kita bypass library eksternal dan gunakan glFrustum langsung
        // Karena glFrustum adalah fungsi core OpenGL 1.1
        typedef void (APIENTRY * PFNGLFRUSTUM)(double left, double right, double bottom, double top, double zNear, double zFar);
        // Casting ke void pointer untuk silence warning jika ada
        glFrustum(-right, right, -top, top, zNear, zFar);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        glRotatef(-camera.pitch * 180.0f / 3.1415926535f, 1.0f, 0.0f, 0.0f);
        glRotatef(-camera.yaw * 180.0f / 3.1415926535f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camera.pos.x, -camera.pos.y, -camera.pos.z);
#endif
    }

private:
    void HandleInput() {
        // Mouselook (Klik Kanan + Geser)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            camera.yaw -= io->MouseDelta.x * 0.005f;
            camera.pitch -= io->MouseDelta.y * 0.005f;
        }

        // WASD Movement
        if (ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) {
            float speed = 0.5f;
            Vec3 forward = camera.GetForward();
            Vec3 right = camera.GetRight();

            if (ImGui::IsKeyDown(ImGuiKey_W)) camera.pos += forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S)) camera.pos -= forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D)) camera.pos += right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A)) camera.pos -= right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) camera.pos.y += speed;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) camera.pos.y -= speed;
        }
    }
};
