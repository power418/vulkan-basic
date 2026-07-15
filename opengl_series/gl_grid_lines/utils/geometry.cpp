#include "geometry.h"
#include <algorithm>

bool Point2D::operator<(const Point2D& p) const {
    return x < p.x || (x == p.x && y < p.y);
}

bool Point2D::operator==(const Point2D& p) const {
    return x == p.x && y == p.y;
}

float cross_product(const Point2D& O, const Point2D& A, const Point2D& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

std::vector<Point2D> ComputeConvexHull(std::vector<Point2D> P) {
    int n = P.size(), k = 0;
    if (n <= 2) return P;
    
    std::vector<Point2D> H(2 * n);

    // Langkah 1: Urutkan titik secara leksikografis
    std::sort(P.begin(), P.end());

    // Langkah 2: Bangun cangkang bawah (Lower Hull)
    for (int i = 0; i < n; ++i) {
        // Hapus titik jika ia tidak membuat belokan ke kiri yang tegas
        while (k >= 2 && cross_product(H[k - 2], H[k - 1], P[i]) <= 0) k--;
        H[k++] = P[i];
    }

    // Langkah 3: Bangun cangkang atas (Upper Hull)
    for (int i = n - 2, t = k + 1; i >= 0; i--) {
        while (k >= t && cross_product(H[k - 2], H[k - 1], P[i]) <= 0) k--;
        H[k++] = P[i];
    }

    // Hapus titik terakhir (karena sama dengan titik awal)
    H.resize(k - 1);
    return H;
}

// ====================================================================
// BAB 11: GEOMETRI 3D
// ====================================================================

std::vector<Face3D> ComputeConvexHull3D(const std::vector<Vec3>& points) {
    std::vector<Face3D> faces;
    int n = points.size();
    if (n < 4) return faces; // Membutuhkan minimal 4 titik (tetrahedron) untuk bentuk 3D

    // Brute-force O(n^4) untuk mencari wajah Convex Hull
    // Cek setiap kemungkinan kombinasi 3 titik (sebagai bidang/segitiga)
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int k = j + 1; k < n; ++k) {
                
                // Cari Vektor Normal dari bidang yang dibentuk titik i, j, k
                Vec3 v1 = points[j] - points[i];
                Vec3 v2 = points[k] - points[i];
                Vec3 normal = Cross(v1, v2);

                // Jika normal mendekati nol, ketiga titik tersebut sejajar (collinear)
                if (normal.LengthSq() < 1e-6f) continue;

                // Cek apakah semua titik SISA berada di satu sisi dari bidang ini
                bool is_face = true;
                bool has_positive = false;
                bool has_negative = false;

                for (int m = 0; m < n; ++m) {
                    if (m == i || m == j || m == k) continue;
                    
                    // Hitung jarak (dot product) titik 'm' ke bidang
                    float d = Dot(normal, points[m] - points[i]);
                    
                    if (d > 1e-4f) has_positive = true;
                    if (d < -1e-4f) has_negative = true;

                    // Jika ada titik di kedua sisi (positif & negatif), berarti bidang ini 
                    // MEMOTONG himpunan titik, sehingga BUKAN bagian terluar dari Convex Hull
                    if (has_positive && has_negative) {
                        is_face = false;
                        break;
                    }
                }

                // Jika semua titik hanya ada di satu sisi, simpan bidang ini!
                if (is_face) {
                    faces.push_back({i, j, k});
                }
            }
        }
    }
    return faces;
}
