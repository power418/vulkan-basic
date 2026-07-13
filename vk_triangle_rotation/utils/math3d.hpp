#pragma once
/*
 * math3d.hpp – utilitas 3D math
 * Menyediakan Vec3, Vec4, Mat4, Vertex untuk transformasi 3D
 * Semua matriks disimpan dalam format row-major: m[row][col]
 */
#include <cmath>
#include <cstring>  // memset

// ------------------------------------------------------------
// Konstanta
// ------------------------------------------------------------
#ifndef M3D_PI
#define M3D_PI      3.14159265358979323846f
#define M3D_TWO_PI  6.28318530717958647692f
#endif

// ------------------------------------------------------------
// Vec3 – vektor 3 komponen (x, y, z)
// ------------------------------------------------------------
struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// ------------------------------------------------------------
// Vec4 – vektor 4 komponen (x, y, z, w)
// ------------------------------------------------------------
struct Vec4 {
    float x, y, z, w;

    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// ------------------------------------------------------------
// Mat4 – matriks 4x4 (row-major: m[row][col])
// ------------------------------------------------------------
struct Mat4 {
    float m[4][4];

    // Konstruktor: semua elemen = 0
    Mat4() { std::memset(m, 0, sizeof(m)); }

    // --------------------------------------------------------
    // Matriks identitas
    // --------------------------------------------------------
    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    // --------------------------------------------------------
    // Matriks proyeksi perspektif untuk Vulkan (NDC z: [0,1])
    //
    // Parameter:
    //   fov    – field of view dalam radian
    //   aspect – rasio lebar / tinggi viewport
    //   n      – jarak bidang dekat (harus > 0)
    //   f      – jarak bidang jauh
    // --------------------------------------------------------
    static Mat4 perspective(float fov, float aspect, float n, float f) {
        Mat4 r;
        float tanHalf = std::tan(fov * 0.5f);
        r.m[0][0] = 1.0f / (aspect * tanHalf);
        r.m[1][1] = 1.0f / tanHalf;
        r.m[2][2] = f / (f - n);              // mapping z_view ke [0,1]
        r.m[2][3] = -n * f / (f - n);          // offset untuk near plane
        r.m[3][2] = 1.0f;                     // perspektif divide (w = z)
        return r;
    }

    // --------------------------------------------------------
    // Matriks rotasi terhadap sumbu Y (sudut dalam radian)
    // --------------------------------------------------------
    static Mat4 rotationY(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    // --------------------------------------------------------
    // Matriks rotasi terhadap sumbu X (sudut dalam radian)
    // --------------------------------------------------------
    static Mat4 rotationX(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        r.m[1][1] = c;  r.m[1][2] = -s;
        r.m[2][1] = s;  r.m[2][2] = c;
        return r;
    }

    // --------------------------------------------------------
    // Matriks rotasi terhadap sumbu Z (sudut dalam radian)
    // --------------------------------------------------------
    static Mat4 rotationZ(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
        return r;
    }

    // --------------------------------------------------------
    // Matriks translasi
    // --------------------------------------------------------
    static Mat4 translation(float x, float y, float z) {
        Mat4 r = identity();
        r.m[0][3] = x;
        r.m[1][3] = y;
        r.m[2][3] = z;
        return r;
    }

    // --------------------------------------------------------
    // Perkalian matriks: this * rhs (standard row * col)
    // --------------------------------------------------------
    Mat4 operator*(const Mat4& rhs) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    r.m[i][j] += m[i][k] * rhs.m[k][j];
        return r;
    }

    // --------------------------------------------------------
    // Transpose: tukar baris <-> kolom
    // --------------------------------------------------------
    Mat4 transpose() const {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }

    // --------------------------------------------------------
    // Pointer ke data mentah (16 float) – untuk dikirim ke GPU
    // --------------------------------------------------------
    const float* data() const { return &m[0][0]; }
};

// ------------------------------------------------------------
// Vertex – format vertex standar untuk pipeline grafis
//   pos[3]   = posisi (x, y, z)
//   color[3] = warna  (r, g, b)
//   uv[2]    = texcoord (u, v)
// ------------------------------------------------------------
struct Vertex {
    float pos[3];
    float color[3];
    float uv[2];
};
