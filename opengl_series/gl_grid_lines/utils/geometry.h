#pragma once
#include <vector>

// --- Bab 11: Geometri 3D & Convex Hull 3D ---
#include "math3d.h"

// --- Bab 1: Geometri Dasar ---
struct Point2D {
    float x, y;

    // Operator urutan untuk memilah titik secara leksikografis (kiri ke kanan, bawah ke atas)
    bool operator<(const Point2D& p) const;
    bool operator==(const Point2D& p) const;
};

// Mendefinisikan sebuah bidang segitiga (wajah luar) dari sebuah Convex Hull 3D
struct Face3D {
    int v[3]; // Indeks dari 3 titik sudut yang membentuk segitiga ini pada array titik asli
};

// Fungsi Predikat Orientasi (Cross Product)
// Menghasilkan nilai positif jika titik O, A, B berbelok ke kiri (Counter-Clockwise)
// Negatif jika berbelok ke kanan, 0 jika sejajar (collinear)
float cross_product(const Point2D& O, const Point2D& A, const Point2D& B);

// Algoritma Convex Hull (Andrew's Monotone Chain) - Bab 1
// Mengembalikan daftar titik Convex Hull secara berurutan berlawanan jarum jam
std::vector<Point2D> ComputeConvexHull(std::vector<Point2D> P);

// Algoritma Convex Hull 3D (Brute-Force O(n^4)) untuk pembelajaran
// Mengembalikan daftar bidang (segitiga) luar yang menyelimuti seluruh titik
std::vector<Face3D> ComputeConvexHull3D(const std::vector<Vec3>& points);
