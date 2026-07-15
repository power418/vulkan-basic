#pragma once
#include <cmath>

// --- Vektor 3D Dasar ---
struct Vec3 {
    float x, y, z;

    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    
    float LengthSq() const { return x*x + y*y + z*z; }
    float Length() const { return std::sqrt(LengthSq()); }
    Vec3 Normalized() const { 
        float len = Length(); 
        return (len > 0.0f) ? (*this / len) : Vec3{0, 0, 0}; 
    }
};

// --- Vektor 4D (Untuk perkalian matriks) ---
struct Vec4 {
    float x, y, z, w;
};

// --- Matriks 4x4 (Column-major untuk OpenGL standar) ---
struct Mat4 {
    float m[4][4]; // m[kolom][baris]

    // Inisialisasi sebagai matriks identitas
    Mat4() {
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // Perkalian Matriks x Matriks
    Mat4 operator*(const Mat4& b) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[c][r] = m[0][r] * b.m[c][0] +
                              m[1][r] * b.m[c][1] +
                              m[2][r] * b.m[c][2] +
                              m[3][r] * b.m[c][3];
            }
        }
        return res;
    }

    // Perkalian Matriks x Vektor 4D
    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z + m[3][0]*v.w,
            m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z + m[3][1]*v.w,
            m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z + m[3][2]*v.w,
            m[0][3]*v.x + m[1][3]*v.y + m[2][3]*v.z + m[3][3]*v.w
        };
    }
};

// --- Struktur Kamera 3D ---
struct Camera3D {
    Vec3 pos = {0.0f, 5.0f, 15.0f};
    float pitch = -0.3f;
    float yaw = 0.0f;
    float fovY = 1.047f; // ~60 derajat dalam radian
    
    // Vektor arah maju
    Vec3 GetForward() const {
        return { std::sin(yaw) * std::cos(pitch), -std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
    }
    
    // Vektor arah samping kanan
    Vec3 GetRight() const {
        return { std::cos(yaw), 0.0f, std::sin(yaw) };
    }

    // Deklarasi fungsi saja (implementasi ada di bawah setelah LookAt)
    Mat4 GetViewMatrix() const;
};

inline float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Fungsi Transformasi Matriks (Translate & Rotate)
inline Mat4 Translate(const Vec3& t) {
    Mat4 res;
    res.m[3][0] = t.x;
    res.m[3][1] = t.y;
    res.m[3][2] = t.z;
    return res;
}

inline Mat4 RotateX(float angle) {
    Mat4 res;
    float c = std::cos(angle), s = std::sin(angle);
    res.m[1][1] = c; res.m[2][1] = -s;
    res.m[1][2] = s; res.m[2][2] = c;
    return res;
}

inline Mat4 RotateY(float angle) {
    Mat4 res;
    float c = std::cos(angle), s = std::sin(angle);
    res.m[0][0] = c; res.m[2][0] = s;
    res.m[0][2] = -s; res.m[2][2] = c;
    return res;
}

// Proyeksi Perspektif
inline Mat4 Perspective(float fovY, float aspect, float nearZ, float farZ) {
    Mat4 res;
    float f = 1.0f / std::tan(fovY * 0.5f);
    // Kosongkan matriks dulu (hapus identitas)
    for(int i=0; i<4; ++i) for(int j=0; j<4; ++j) res.m[i][j] = 0.0f;
    
    res.m[0][0] = f / aspect;
    res.m[1][1] = f;
    res.m[2][2] = (farZ + nearZ) / (nearZ - farZ);
    res.m[2][3] = -1.0f;
    res.m[3][2] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return res;
}

// View Matrix (LookAt)
inline Mat4 LookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = (center - eye).Normalized();
    Vec3 s = Cross(f, up).Normalized();
    Vec3 u = Cross(s, f);

    Mat4 res;
    res.m[0][0] = s.x; res.m[1][0] = s.y; res.m[2][0] = s.z;
    res.m[0][1] = u.x; res.m[1][1] = u.y; res.m[2][1] = u.z;
    res.m[0][2] = -f.x; res.m[1][2] = -f.y; res.m[2][2] = -f.z;
    res.m[3][0] = -Dot(s, eye);
    res.m[3][1] = -Dot(u, eye);
    res.m[3][2] = Dot(f, eye);
    return res;
}

// --- Implementasi Fungsi Struct (Di luar deklarasi) ---
// Teknik ini digunakan agar Camera3D bisa memanggil LookAt() yang baru 
// didefinisikan di baris bawah setelah Camera3D.
inline Mat4 Camera3D::GetViewMatrix() const {
    Vec3 target = pos + GetForward();
    return LookAt(pos, target, {0.0f, 1.0f, 0.0f});
}
