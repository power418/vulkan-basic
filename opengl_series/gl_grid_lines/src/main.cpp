#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <GL/gl.h> // REQUIRED FOR NATIVE OPENGL
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "../utils/device.h"
#include "../utils/geometry.h"
#include "../utils/infinity_grid.h"
#include "../utils/math3d.h"
#include "../utils/selection.h"
#include "../utils/viewport3d.h"
#include "mesh.h"

// ========================================================================
// GLOBAL STATE & RENDER HELPERS
// ========================================================================
Viewport3D g_viewport;
bool g_is_raycast_hit = false;
Vec3 g_hit_pos;
float g_hit_distance = 0.0f;
std::vector<Vec3> freeDrawPoints3D;
bool g_render_native_3d = false;

void DrawLineGL(Vec3 a, Vec3 b, ImU32 color, float thickness = 2.0f) {
  float r = ((color >> 0) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b_col = ((color >> 16) & 0xFF) / 255.0f;

  glLineWidth(thickness);
  glColor3f(r, g, b_col);
  glBegin(GL_LINES);
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
}

void DrawCubeGL(Vec3 pos, float size, ImU32 color) {
  float h = size * 0.5f;
  float r = ((color >> 0) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b_col = ((color >> 16) & 0xFF) / 255.0f;

  glPushMatrix();
  glTranslatef(pos.x, pos.y, pos.z);
  glColor3f(r, g, b_col);

  glBegin(GL_QUADS);
  // Front Face
  glVertex3f(-h, -h, h);
  glVertex3f(h, -h, h);
  glVertex3f(h, h, h);
  glVertex3f(-h, h, h);
  // Back Face
  glVertex3f(-h, -h, -h);
  glVertex3f(-h, h, -h);
  glVertex3f(h, h, -h);
  glVertex3f(h, -h, -h);
  // Top Face
  glVertex3f(-h, h, -h);
  glVertex3f(-h, h, h);
  glVertex3f(h, h, h);
  glVertex3f(h, h, -h);
  // Bottom Face
  glVertex3f(-h, -h, -h);
  glVertex3f(h, -h, -h);
  glVertex3f(h, -h, h);
  glVertex3f(-h, -h, h);
  // Right face
  glVertex3f(h, -h, -h);
  glVertex3f(h, h, -h);
  glVertex3f(h, h, h);
  glVertex3f(h, -h, h);
  // Left Face
  glVertex3f(-h, -h, -h);
  glVertex3f(-h, -h, h);
  glVertex3f(-h, h, h);
  glVertex3f(-h, h, -h);
  glEnd();

  glPopMatrix();
}

// ========================================================================
// APP RUNNER (TEMPLATE)
// ========================================================================

static void glfw_error_callback(int error, const char *description) {
  std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

template <typename AppCallback>
int RunImGuiApp(const char *window_title, AppCallback app_logic) {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

#if defined(__APPLE__)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow *window = glfwCreateWindow(1280, 720, window_title, nullptr, nullptr);

  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // VSync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Cetak Informasi GPU ke Console
  std::cout << "=================================================\n";
  std::cout << "[INFO] OpenGL Context Created Successfully!\n";
  std::cout << "[INFO] Active GPU: " << DeviceManager::GetActiveGPUName()
            << "\n";
  std::cout << "=================================================\n";

  ImVec4 clear_color = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);

  int frame_count = 0;
  double last_time = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // LOGGING KE CONSOLE (Setiap 1 detik agar terminal tidak kebanjiran teks)
    frame_count++;
    double current_time = glfwGetTime();
    if (current_time - last_time >= 1.0) {
      std::cout << "[LOOP LOG] Engine running 100% on: "
                << DeviceManager::GetActiveGPUName() << " | Rendering at "
                << frame_count << " FPS\n";
      frame_count = 0;
      last_time = current_time;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ------------------------------------------
    // Panggil logika UI spesifik Aplikasi
    app_logic(io, window);
    // ------------------------------------------

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    // Bersihkan Warna dan Depth (Z-Buffer)
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- NATIVE OPENGL 3D RENDERING ---
    if (g_render_native_3d) {
      glEnable(GL_DEPTH_TEST);
      g_viewport.SetupOpenGLMatrices((float)display_w, (float)display_h);

      // 1. Gambar Lantai Grid Infinite
      float step = 5.0f;
      float view_distance = 200.0f;
      float center_x = std::floor(g_viewport.camera.pos.x / step) * step;
      float center_z = std::floor(g_viewport.camera.pos.z / step) * step;

      for (float x = center_x - view_distance; x <= center_x + view_distance; x += step) {
        ImU32 col = IM_COL32(150, 150, 150, 60);
        if (std::abs(x) < 0.1f) {
            col = IM_COL32(100, 255, 100, 200); // Warna Hijau (Sumbu Depth/Maju Mundur)
        }
        DrawLineGL({x, 0.0f, center_z - view_distance},
                   {x, 0.0f, center_z + view_distance}, col, 1.0f);
      }
      
      for (float z = center_z - view_distance; z <= center_z + view_distance; z += step) {
        ImU32 col = IM_COL32(150, 150, 150, 60);
        if (std::abs(z) < 0.1f) {
            col = IM_COL32(255, 100, 100, 200); // Warna Merah (Sumbu Horizontal/Kiri Kanan)
        }
        DrawLineGL({center_x - view_distance, 0.0f, z},
                   {center_x + view_distance, 0.0f, z}, col, 1.0f);
      }

      // Sumbu Y Vertikal (Biru)
      DrawLineGL({0.0f, -view_distance, 0.0f}, {0.0f, view_distance, 0.0f}, IM_COL32(100, 100, 255, 200), 1.0f);

      // 2. Gambar Jalur Garis Bebas 3D (Cubes and Lines)
      if (freeDrawPoints3D.size() >= 2) {
        for (size_t i = 0; i < freeDrawPoints3D.size() - 1; ++i) {
          DrawLineGL(freeDrawPoints3D[i], freeDrawPoints3D[i + 1],
                     IM_COL32(0, 255, 255, 255), 3.0f);
        }
      }
      for (const auto &pt : freeDrawPoints3D) {
        DrawCubeGL(pt, 0.5f, IM_COL32(255, 255, 0, 255)); // Render sbg Kubus 3D
      }

      // 3. Gambar Cursor/Pilar Raycast saat ini (jika menyentuh lantai)
      if (g_is_raycast_hit) {
        DrawLineGL(g_hit_pos, {g_hit_pos.x, g_hit_pos.y + 5.0f, g_hit_pos.z},
                   IM_COL32(255, 50, 50, 255), 3.0f);
        DrawCubeGL(g_hit_pos, 0.8f, IM_COL32(255, 50, 50, 255));
      }

      glDisable(GL_DEPTH_TEST);
    }
    // --- SELESAI NATIVE OPENGL ---

    // ImGui di atas segalanya (UI layer)
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

// ========================================================================
// 2D CONVEX HULL APP (BAB 1)
// ========================================================================

struct {
  float panX = 0.0f, panY = 0.0f;
  float zoom = 1.0f;
} state2D;
std::vector<Point2D> points2D;

void ShowConvexHull2DApp(ImGuiIO &io, GLFWwindow *window) {
  g_render_native_3d = false;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Canvas2D", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImVec2 sz = ImGui::GetContentRegionAvail();
  ImVec2 p1 = ImVec2(p0.x + sz.x, p0.y + sz.y);

  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  static SelectionState marqueeSelection;

  // Fungsi Handler untuk menghapus titik yang sedang terseleksi
  auto RemoveSelectedPoints = [&]() {
    if (marqueeSelection.selected_indices.empty())
      return;

    // Sort descending agar index tidak bergeser saat elemen dihapus dari vector
    std::vector<int> indices_to_remove = marqueeSelection.selected_indices;
    std::sort(indices_to_remove.rbegin(), indices_to_remove.rend());

    for (int idx : indices_to_remove) {
      if (idx >= 0 && idx < points2D.size()) {
        points2D.erase(points2D.begin() + idx);
      }
    }
    marqueeSelection.selected_indices.clear();
  };

  // Deteksi tombol 'Delete' untuk menghapus
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    RemoveSelectedPoints();
  }

  if (ImGui::IsWindowHovered()) {
    // Panning (Geser Canvas): Klik Tengah & Tarik
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
      state2D.panX += io.MouseDelta.x;
      state2D.panY += io.MouseDelta.y;
    }
    // Zoom: Scroll Mouse
    if (io.MouseWheel != 0.0f) {
      state2D.zoom += io.MouseWheel * 0.1f;
      if (state2D.zoom < 0.1f)
        state2D.zoom = 0.1f;
    }

    // Tambah Titik (Point Geometry): Klik Kanan
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      Point2D pt;
      pt.x = (io.MousePos.x - p0.x - state2D.panX - sz.x * 0.5f) / state2D.zoom;
      pt.y = (io.MousePos.y - p0.y - state2D.panY - sz.y * 0.5f) / state2D.zoom;
      if (points2D.empty() || !(points2D.back() == pt)) {
        points2D.push_back(pt);
      }
    }

    // Marquee Selection: Klik Kiri Hold
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      marqueeSelection.BeginSelection(io.MousePos);
    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      marqueeSelection.UpdateSelection(io.MousePos);
    }
  }

  // Akhiri seleksi saat klik kiri dilepas, meskipun kursor di luar window ImGui
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    marqueeSelection.EndSelection();
  }

  // [Geometry & Algebra] Infinite Grid Rendering menggunakan fungsi Modulo
  // fmodf(pan, grid_step) membatasi pergeseran layar tetap dalam rentang 1
  // kotak grid. Ini menciptakan ilusi grid 2D yang tak terbatas (infinite) saat
  // kamera digeser (panning).
  float grid_step = 50.0f * state2D.zoom;
  if (grid_step > 5.0f) {
    for (float x = fmodf(state2D.panX, grid_step); x < sz.x; x += grid_step)
      draw_list->AddLine(ImVec2(p0.x + x, p0.y), ImVec2(p0.x + x, p1.y),
                         IM_COL32(200, 200, 200, 40));
    for (float y = fmodf(state2D.panY, grid_step); y < sz.y; y += grid_step)
      draw_list->AddLine(ImVec2(p0.x, p0.y + y), ImVec2(p1.x, p0.y + y),
                         IM_COL32(200, 200, 200, 40));
  }

  ImVec2 origin(p0.x + state2D.panX + sz.x * 0.5f,
                p0.y + state2D.panY + sz.y * 0.5f);
  draw_list->AddLine(ImVec2(origin.x, p0.y), ImVec2(origin.x, p1.y),
                     IM_COL32(255, 100, 100, 150), 2.0f);
  draw_list->AddLine(ImVec2(p0.x, origin.y), ImVec2(p1.x, origin.y),
                     IM_COL32(100, 255, 100, 150), 2.0f);

  // [Linear Algebra] Affine Transformation (Transformasi Ruang/Basis)
  // Mengubah koordinat Dunia (World Space) ke koordinat Layar (Screen Space).
  // Operasi matematikanya: Screen_X = Origin_X + (World_X * Scale) +
  // Translation_X
  auto WorldToScreen = [&](const Point2D &p) -> ImVec2 {
    return ImVec2(p0.x + sz.x * 0.5f + state2D.panX + p.x * state2D.zoom,
                  p0.y + sz.y * 0.5f + state2D.panY + p.y * state2D.zoom);
  };

  // [Computational Geometry] Convex Hull
  // Membungkus seluruh himpunan titik 2D dengan poligon cembung terkecil.
  // Perhitungannya biasa menggunakan dot product/cross product (seperti Graham
  // Scan atau Monotone Chain) untuk memastikan setiap belokan sudut terluar
  // tidak pernah membentuk sudut cekung.
  std::vector<Point2D> hull = ComputeConvexHull(points2D);
  if (hull.size() > 1) {
    for (size_t i = 0; i < hull.size(); ++i) {
      ImVec2 sp1 = WorldToScreen(hull[i]);
      ImVec2 sp2 = WorldToScreen(hull[(i + 1) % hull.size()]);
      draw_list->AddLine(sp1, sp2, IM_COL32(50, 255, 255, 255), 3.0f);
    }
  }

  static float mesh_opacity = 0.5f;
  if (hull.size() >= 3) {
    Mesh2D mesh;
    mesh.setOpacity(mesh_opacity);
    std::vector<unsigned int> all_v_indices;
    for (const auto &p : points2D) {
      all_v_indices.push_back(mesh.addVertex(p.x, p.y));
    }

    std::vector<unsigned int> hull_indices;
    for (const auto &hp : hull) {
      for (size_t i = 0; i < points2D.size(); ++i) {
        if (points2D[i] == hp) {
          hull_indices.push_back(static_cast<unsigned int>(i));
          break;
        }
      }
    }

    // Buat boundary otomatis (Triangulasi Ear Clipping)
    for (size_t i = 0; i < hull_indices.size(); ++i) {
      mesh.addEdge(hull_indices[i],
                   hull_indices[(i + 1) % hull_indices.size()]);
    }

    // Masukkan titik-titik interior yang ada di dalam bidang agar jaringnya
    // tersambung (Delaunay-like split)
    for (size_t i = 0; i < points2D.size(); ++i) {
      bool is_hull = false;
      for (auto hi : hull_indices) {
        if (hi == i) {
          is_hull = true;
          break;
        }
      }
      if (!is_hull) {
        mesh.insertInteriorPoint(static_cast<unsigned int>(i));
      }
    }

    // Draw Faces
    const auto &verts = mesh.getVertices();
    for (const auto &face : mesh.getFaces()) {
      ImVec2 p1 = WorldToScreen({verts[face.v1].x, verts[face.v1].y});
      ImVec2 p2 = WorldToScreen({verts[face.v2].x, verts[face.v2].y});
      ImVec2 p3 = WorldToScreen({verts[face.v3].x, verts[face.v3].y});

      draw_list->AddTriangleFilled(
          p1, p2, p3, IM_COL32(0, 255, 100, (int)(mesh.getOpacity() * 255.0f)));
      draw_list->AddTriangle(
          p1, p2, p3, IM_COL32(0, 200, 50, (int)(mesh.getOpacity() * 255.0f)),
          1.0f); // Wireframe internal
    }
  }

  // Kumpulkan posisi screen dari titik-titik untuk Hit Test Seleksi
  std::vector<ImVec2> screen_points(points2D.size());
  for (size_t i = 0; i < points2D.size(); ++i) {
    screen_points[i] = WorldToScreen(points2D[i]);
  }

  // Update Hit Test jika sedang melakukan seleksi marquee
  if (marqueeSelection.is_selecting) {
    marqueeSelection.PerformHitTest(screen_points);
  }

  // Gambar titik-titik (dengan warna berbeda jika terseleksi)
  for (size_t i = 0; i < points2D.size(); ++i) {
    ImU32 col = IM_COL32(255, 255, 0, 255); // Default kuning
    if (std::find(marqueeSelection.selected_indices.begin(),
                  marqueeSelection.selected_indices.end(),
                  i) != marqueeSelection.selected_indices.end()) {
      col = IM_COL32(255, 50, 50, 255); // Merah saat terseleksi
    }
    draw_list->AddCircleFilled(screen_points[i], 5.0f, col);
  }

  // Gambar Kotak Marquee Selection di atas (Foreground) titik
  marqueeSelection.DrawMarquee(draw_list);

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::Begin("Controls 2D", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Total Titik Input: %d", (int)points2D.size());
  ImGui::Text("Sisi Convex Hull: %d", (int)hull.size());
  if (ImGui::Button("Hapus Semua Titik", ImVec2(-1, 0)))
    points2D.clear();

  ImGui::BeginDisabled(marqueeSelection.selected_indices.empty());
  if (ImGui::Button("Hapus Titik Terseleksi (Del)", ImVec2(-1, 0))) {
    RemoveSelectedPoints();
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::SliderFloat("Opacity Bidang", &mesh_opacity, 0.0f, 1.0f);

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Kontrol Canvas:");
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                     "- Geser (Pan)    : Klik Tengah & Tarik");
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                     "- Zoom           : Scroll Mouse");
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                     "- Tambah Titik   : Klik Kanan");
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                     "- Marquee Select : Tahan Klik Kiri");
  ImGui::End();
}

// ========================================================================
// 3D FREE DRAW APP (FUNGSI GAMBAR BEBAS SEPERTI 2D)
// ========================================================================
void ShowFreeDraw3DApp(ImGuiIO &io, GLFWwindow *window) {
  g_render_native_3d = true;
  g_viewport.BeginRender("Canvas3D Free Draw", io);

  g_is_raycast_hit = g_viewport.RaycastToGround(g_hit_pos, false);
  if (g_is_raycast_hit) {

    g_hit_distance = (g_hit_pos - g_viewport.camera.pos).Length();

    // Debug: Gambar Crosshair 2D di UI layer (tetap memakai ImGui canvas)
    ImVec2 mpos = ImGui::GetMousePos();
    g_viewport.draw_list->AddLine({mpos.x - 15, mpos.y}, {mpos.x + 15, mpos.y},
                                  IM_COL32(0, 255, 0, 255), 2.0f);
    g_viewport.draw_list->AddLine({mpos.x, mpos.y - 15}, {mpos.x, mpos.y + 15},
                                  IM_COL32(0, 255, 0, 255), 2.0f);
    g_viewport.draw_list->AddText(
        {mpos.x + 15, mpos.y - 15}, IM_COL32(0, 255, 0, 255),
        ("Jarak: " + std::to_string((int)g_hit_distance) + "m").c_str());

    // KINI KITA BISA HOLD KLIK KIRI UNTUK MENGGAMBAR KONTINU (SEPERTI PENSIL)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      // Batasi agar tidak menaruh titik terlalu jauh di cakrawala (horizon)
      if (g_hit_distance < 500.0f) {
        // Tambahkan titik jika belum ada, ATAU jika jarak dari titik sebelumnya
        // > 2.0 meter
        if (freeDrawPoints3D.empty() ||
            (g_hit_pos - freeDrawPoints3D.back()).Length() > 2.0f) {
          freeDrawPoints3D.push_back(g_hit_pos);
        }
      }
    }
  }

  g_viewport.EndRender();

  ImGui::Begin("Free Draw 3D Controls", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Mode: NATIVE OpenGL 3D Render");
  ImGui::TextColored(ImVec4(0, 1, 1, 1), "Cara Pakai:");
  ImGui::Text("- TAHAN KLIK KIRI: Menggambar Kontinu");
  ImGui::Text("- KLIK KANAN + GESER: Mouselook (Putar Kamera)");
  ImGui::Text("- W, A, S, D: Bergerak Maju/Mundur/Kiri/Kanan");
  ImGui::Separator();
  ImGui::Text("Posisi Kamera Asli: (%.1f, %.1f, %.1f)", g_viewport.camera.pos.x,
              g_viewport.camera.pos.y, g_viewport.camera.pos.z);
  ImGui::Text("Total Titik Jalur: %d", (int)freeDrawPoints3D.size());
  if (ImGui::Button("Clear Jalur", ImVec2(-1, 0))) {
    freeDrawPoints3D.clear();
  }
  ImGui::End();
}

// ========================================================================
// APP SWITCHER & MAIN ENTRY POINT
// ========================================================================

enum class AppMode { Mode2D, Mode3D };
AppMode current_mode = AppMode::Mode2D;

void MasterAppLogic(ImGuiIO &io, GLFWwindow *window) {
  // 1. Jalankan aplikasi berdasarkan mode yang dipilih
  if (current_mode == AppMode::Mode2D) {
    ShowConvexHull2DApp(io, window);
  } else {
    ShowFreeDraw3DApp(io, window);
  }

  // 2. Gambar Panel Switcher di atas (Floating)
  // Posisi di tengah atas layar
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 115.0f, 10.0f),
                          ImGuiCond_Always);
  ImGui::Begin("App Switcher", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings);

  // Beri warna hijau jika mode 2D aktif
  if (current_mode == AppMode::Mode2D)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  else
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyle().Colors[ImGuiCol_Button]);
  if (ImGui::Button("2D Mode", ImVec2(100, 30)))
    current_mode = AppMode::Mode2D;
  ImGui::PopStyleColor();

  ImGui::SameLine(0.0f, 15.0f);

  // Beri warna hijau jika mode 3D aktif
  if (current_mode == AppMode::Mode3D)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
  else
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyle().Colors[ImGuiCol_Button]);
  if (ImGui::Button("3D Mode", ImVec2(100, 30)))
    current_mode = AppMode::Mode3D;
  ImGui::PopStyleColor();

  ImGui::End();
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  // Menjalankan MasterAppLogic yang membungkus 2D dan 3D app
  return RunImGuiApp("2D & 3D Switcher App", MasterAppLogic);
}
