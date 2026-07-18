#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <imgui.h>

struct SelectionState {
    bool is_selecting = false;
    ImVec2 start_pos;
    ImVec2 current_pos;
    
    // Indeks titik-titik yang terseleksi
    std::vector<int> selected_indices;

    void BeginSelection(ImVec2 start) {
        is_selecting = true;
        start_pos = start;
        current_pos = start;
        selected_indices.clear();
    }

    void UpdateSelection(ImVec2 current) {
        if (is_selecting) {
            current_pos = current;
        }
    }

    void EndSelection() {
        is_selecting = false;
    }

    // Menggambar dashed line box (Marquee Selection) ala OS Windows
    void DrawMarquee(ImDrawList* draw_list) {
        if (!is_selecting) return;

        ImVec2 min_p(std::min(start_pos.x, current_pos.x), std::min(start_pos.y, current_pos.y));
        ImVec2 max_p(std::max(start_pos.x, current_pos.x), std::max(start_pos.y, current_pos.y));

        // 1. Background biru transparan khas OS Windows
        draw_list->AddRectFilled(min_p, max_p, IM_COL32(0, 120, 215, 50));
        
        // 2. Dashed line border
        float dash_length = 4.0f;
        float gap_length = 4.0f;
        ImU32 color = IM_COL32(0, 120, 215, 255); // Biru Windows
        float thickness = 1.0f;

        // Helper fungsi untuk menggambar garis putus-putus
        auto DrawDashedLine = [&](ImVec2 p1, ImVec2 p2) {
            ImVec2 dir(p2.x - p1.x, p2.y - p1.y);
            float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (length < 0.001f) return;
            dir.x /= length;
            dir.y /= length;

            float current_dist = 0.0f;
            while (current_dist < length) {
                float next_dash = std::min(current_dist + dash_length, length);
                draw_list->AddLine(
                    ImVec2(p1.x + dir.x * current_dist, p1.y + dir.y * current_dist),
                    ImVec2(p1.x + dir.x * next_dash, p1.y + dir.y * next_dash),
                    color, thickness
                );
                current_dist = next_dash + gap_length;
            }
        };

        DrawDashedLine(ImVec2(min_p.x, min_p.y), ImVec2(max_p.x, min_p.y)); // Atas
        DrawDashedLine(ImVec2(max_p.x, min_p.y), ImVec2(max_p.x, max_p.y)); // Kanan
        DrawDashedLine(ImVec2(max_p.x, max_p.y), ImVec2(min_p.x, max_p.y)); // Bawah
        DrawDashedLine(ImVec2(min_p.x, max_p.y), ImVec2(min_p.x, min_p.y)); // Kiri
    }

    // Melakukan hit test (pengecekan titik mana yang masuk area marquee)
    void PerformHitTest(const std::vector<ImVec2>& screen_points) {
        selected_indices.clear();
        ImVec2 min_p(std::min(start_pos.x, current_pos.x), std::min(start_pos.y, current_pos.y));
        ImVec2 max_p(std::max(start_pos.x, current_pos.x), std::max(start_pos.y, current_pos.y));

        for (int i = 0; i < screen_points.size(); ++i) {
            const auto& pt = screen_points[i];
            if (pt.x >= min_p.x && pt.x <= max_p.x && pt.y >= min_p.y && pt.y <= max_p.y) {
                selected_indices.push_back(i);
            }
        }
    }
};
