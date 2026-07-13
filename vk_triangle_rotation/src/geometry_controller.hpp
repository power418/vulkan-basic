#pragma once
/*
 * geometry_controller.hpp – Generator geometri poligon
 * Menghasilkan vertex list (triangle list) untuk N-gon datar,
 * dari segitiga (3 sisi) sampai lingkaran (≥36 sisi).
 */
#include "../utils/math3d.hpp"
#include <vector>

class GeometryController {
public:
    // ---------------------------------------------------------------
    // generatePolygon  – menghasilkan N-gon datar pada bidang XY
    //
    // Parameter:
    //   sides  – jumlah sisi (min 3, max 360)
    //   radius – radius poligon (default 1.0)
    //
    // Return:
    //   std::vector<Vertex> berisi triangle list (fan dari pusat)
    //   Total vertices = sides * 3
    // ---------------------------------------------------------------
    static std::vector<Vertex> generatePolygon(int sides, float radius = 1.0f);
};
