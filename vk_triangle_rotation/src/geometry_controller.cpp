#include "geometry_controller.hpp"
#include <cmath>

/*
 * generatePolygon – Membuat piramida 3D dengan alas N-gon.
 *
 * Struktur:
 *   - Apex (puncak) di (0, -1, 0)
 *   - Alas: N titik melingkar di y = 1, radius pada bidang XZ
 *   - Sisi samping: N segitiga (apex → base[i] → base[i+1])
 *   - Alas bawah: N segitiga fan dari pusat alas (0, 1, 0)
 *
 * Total vertices = N * 3 (sisi) + N * 3 (alas) = N * 6
 */
std::vector<Vertex> GeometryController::generatePolygon(int sides, float radius) {
    // Clamp sides ke range [3, 360]
    if (sides < 3)   sides = 3;
    if (sides > 360)  sides = 360;

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>(sides) * 6);

    float angleStep = M3D_TWO_PI / static_cast<float>(sides);

    // Posisi apex (puncak piramida)
    float apexY = -1.0f;
    // Posisi alas
    float baseY = 1.0f;

    // ================================================================
    // SISI SAMPING: N segitiga dari apex ke setiap edge alas
    // ================================================================
    for (int i = 0; i < sides; ++i) {
        float angle0 = angleStep * static_cast<float>(i);
        float angle1 = angleStep * static_cast<float>(i + 1);

        // Posisi base vertices pada bidang XZ
        float x0 = radius * std::cos(angle0);
        float z0 = radius * std::sin(angle0);
        float x1 = radius * std::cos(angle1);
        float z1 = radius * std::sin(angle1);

        // Gradien warna per-sisi (pastel pink-like, mirip aslinya)
        float t = static_cast<float>(i) / static_cast<float>(sides);
        float r = 0.82f + 0.18f * std::cos(M3D_TWO_PI * t);
        float g = 0.50f + 0.25f * std::cos(M3D_TWO_PI * t + 2.094f);
        float b = 0.58f + 0.22f * std::cos(M3D_TWO_PI * t + 4.189f);

        // UV mapping: apex di atas tengah, base vertices di bawah
        // Apex
        Vertex vApex{};
        vApex.pos[0] = 0.0f;
        vApex.pos[1] = apexY;
        vApex.pos[2] = 0.0f;
        vApex.color[0] = r;
        vApex.color[1] = g;
        vApex.color[2] = b;
        vApex.uv[0] = 0.50f;
        vApex.uv[1] = 0.05f;

        // Base vertex 0 (Kiri dari sudut pandang face)
        Vertex vB0{};
        vB0.pos[0] = x0;
        vB0.pos[1] = baseY;
        vB0.pos[2] = z0;
        vB0.color[0] = r;
        vB0.color[1] = g;
        vB0.color[2] = b;
        vB0.uv[0] = 0.20f;
        vB0.uv[1] = 0.65f;

        // Base vertex 1 (Kanan dari sudut pandang face)
        Vertex vB1{};
        vB1.pos[0] = x1;
        vB1.pos[1] = baseY;
        vB1.pos[2] = z1;
        vB1.color[0] = r;
        vB1.color[1] = g;
        vB1.color[2] = b;
        vB1.uv[0] = 0.80f;
        vB1.uv[1] = 0.65f;

        // Segitiga sisi: apex → base1 → base0 (winding order clockwise)
        vertices.push_back(vApex);
        vertices.push_back(vB1);
        vertices.push_back(vB0);
    }

    // ================================================================
    // ALAS BAWAH: N segitiga fan dari pusat alas ke perimeter
    // ================================================================
    // Warna alas lebih gelap
    float baseR = 0.75f;
    float baseG = 0.42f;
    float baseB = 0.50f;

    Vertex vCenter{};
    vCenter.pos[0] = 0.0f;
    vCenter.pos[1] = baseY;
    vCenter.pos[2] = 0.0f;
    vCenter.color[0] = baseR;
    vCenter.color[1] = baseG;
    vCenter.color[2] = baseB;
    vCenter.uv[0] = 0.50f;
    vCenter.uv[1] = 0.42f;

    for (int i = 0; i < sides; ++i) {
        float angle0 = angleStep * static_cast<float>(i);
        float angle1 = angleStep * static_cast<float>(i + 1);

        float x0 = radius * std::cos(angle0);
        float z0 = radius * std::sin(angle0);
        float x1 = radius * std::cos(angle1);
        float z1 = radius * std::sin(angle1);

        // UV mapping alas menggunakan posisi pada lingkaran
        float u0 = 0.20f + 0.60f * (0.5f + 0.5f * std::cos(angle0));
        float v0 = 0.20f + 0.45f * (0.5f + 0.5f * std::sin(angle0));
        float u1 = 0.20f + 0.60f * (0.5f + 0.5f * std::cos(angle1));
        float v1 = 0.20f + 0.45f * (0.5f + 0.5f * std::sin(angle1));

        Vertex vP0{};
        vP0.pos[0] = x0;
        vP0.pos[1] = baseY;
        vP0.pos[2] = z0;
        vP0.color[0] = baseR;
        vP0.color[1] = baseG;
        vP0.color[2] = baseB;
        vP0.uv[0] = u0;
        vP0.uv[1] = v0;

        Vertex vP1{};
        vP1.pos[0] = x1;
        vP1.pos[1] = baseY;
        vP1.pos[2] = z1;
        vP1.color[0] = baseR;
        vP1.color[1] = baseG;
        vP1.color[2] = baseB;
        vP1.uv[0] = u1;
        vP1.uv[1] = v1;

        // Segitiga alas: center → p1 → p0 (clockwise dilihat dari bawah)
        vertices.push_back(vCenter);
        vertices.push_back(vP1);
        vertices.push_back(vP0);
    }

    return vertices;
}
