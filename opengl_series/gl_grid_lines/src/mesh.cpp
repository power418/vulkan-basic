#include "mesh.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

unsigned int Mesh2D::addVertex(float x, float y) {
    vertices.push_back({x, y});
    return static_cast<unsigned int>(vertices.size() - 1);
}

void Mesh2D::addEdge(unsigned int v1, unsigned int v2) {
    edges.push_back({v1, v2});
    processTriangulation(); // Coba generate mesh otomatis jika membentuk bidang
}

const std::vector<Vertex2D>& Mesh2D::getVertices() const {
    return vertices;
}

const std::vector<Edge>& Mesh2D::getEdges() const {
    return edges;
}

const std::vector<Face>& Mesh2D::getFaces() const {
    return faces;
}

void Mesh2D::clear() {
    vertices.clear();
    edges.clear();
    faces.clear();
}

bool Mesh2D::insertInteriorPoint(unsigned int vIndex) {
    if (vIndex >= vertices.size()) return false;
    const Vertex2D& p = vertices[vIndex];

    for (size_t i = 0; i < faces.size(); ++i) {
        const Face& f = faces[i];
        const Vertex2D& a = vertices[f.v1];
        const Vertex2D& b = vertices[f.v2];
        const Vertex2D& c = vertices[f.v3];

        if (isPointInTriangle(p, a, b, c)) {
            // Kita temukan segitiga yang memuat titik P.
            // Buat 3 segitiga baru untuk menghubungkan titik P
            Face f1 = {f.v1, f.v2, vIndex};
            Face f2 = {f.v2, f.v3, vIndex};
            Face f3 = {f.v3, f.v1, vIndex};

            // Ganti face lama dengan f1, dan tambahkan f2, f3
            faces[i] = f1;
            faces.push_back(f2);
            faces.push_back(f3);
            return true;
        }
    }
    return false;
}

void Mesh2D::setOpacity(float o) {
    opacity = std::clamp(o, 0.0f, 1.0f);
}

float Mesh2D::getOpacity() const {
    return opacity;
}

void Mesh2D::setColor(float r, float g, float b) {
    colorR = std::clamp(r, 0.0f, 1.0f);
    colorG = std::clamp(g, 0.0f, 1.0f);
    colorB = std::clamp(b, 0.0f, 1.0f);
}

void Mesh2D::getColor(float& r, float& g, float& b) const {
    r = colorR;
    g = colorG;
    b = colorB;
}

std::vector<unsigned int> Mesh2D::findClosedLoop() {
    if (edges.empty()) return {};

    // Membangun adjacency list undirected
    std::unordered_map<unsigned int, std::vector<unsigned int>> adj;
    for (const auto& edge : edges) {
        adj[edge.v1].push_back(edge.v2);
        adj[edge.v2].push_back(edge.v1);
    }

    // Melacak cycle / bidang tertutup sederhana
    std::vector<unsigned int> loop;
    std::unordered_set<unsigned int> visited;
    
    // Kita mulai penelusuran dari node pada edge pertama
    unsigned int startNode = edges[0].v1;
    unsigned int currNode = startNode;
    
    loop.push_back(currNode);
    visited.insert(currNode);

    while (true) {
        bool moved = false;
        for (unsigned int neighbor : adj[currNode]) {
            // Jika kembali ke awal dan node sudah >= 3, artinya membentuk bidang utuh
            if (neighbor == startNode && loop.size() >= 3) {
                return loop;
            }
            // Jika belum dikunjungi, lanjut ke node tersebut
            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                loop.push_back(neighbor);
                currNode = neighbor;
                moved = true;
                break;
            }
        }
        // Jika stuck (ujung buntu atau terputus), berarti tidak terbentuk loop
        if (!moved) {
            break; 
        }
    }
    
    return {};
}

// [Linear Algebra] 2D Cross Product (Determinant)
// Menghitung besar dari perkalian silang (cross product) antara vektor AB dan AC.
// Secara matematis, ini adalah determinan matriks 2x2: |(b.x - a.x)  (c.x - a.x)|
//                                                      |(b.y - a.y)  (c.y - a.y)|
// Nilai ini merepresentasikan Luas Berarah (Signed Area) dari jajargenjang yang dibentuk.
// Jika hasil > 0, arah putarannya Counter-Clockwise (CCW) / ke kiri.
// Jika hasil < 0, arah putarannya Clockwise (CW) / ke kanan.
float Mesh2D::crossProduct(const Vertex2D& a, const Vertex2D& b, const Vertex2D& c) const {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// [Linear Algebra] Point-in-Triangle Test menggunakan Half-Space / Barycentric
bool Mesh2D::isPointInTriangle(const Vertex2D& p, const Vertex2D& a, const Vertex2D& b, const Vertex2D& c) const {
    // Menghitung di sisi mana titik P berada terhadap masing-masing dari 3 garis segitiga (AB, BC, CA).
    float cp1 = crossProduct(a, b, p);
    float cp2 = crossProduct(b, c, p);
    float cp3 = crossProduct(c, a, p);
    
    // Titik P berada di dalam segitiga jika ia berada di sisi yang sama 
    // dari ketiga garis (artinya, ketiga cross product tidak ada yang berlawanan tanda).
    bool has_neg = (cp1 < 0) || (cp2 < 0) || (cp3 < 0);
    bool has_pos = (cp1 > 0) || (cp2 > 0) || (cp3 > 0);
    
    return !(has_neg && has_pos);
}

bool Mesh2D::isEar(size_t prev, size_t curr, size_t next, const std::vector<unsigned int>& polygon) const {
    const Vertex2D& p = vertices[polygon[prev]];
    const Vertex2D& c = vertices[polygon[curr]];
    const Vertex2D& n = vertices[polygon[next]];

    // [Linear Algebra] Cek Convexity menggunakan Cross Product.
    // Kita asumsikan urutan polygon adalah Counter-Clockwise (CCW).
    // Jika cross product (vektor PC x vektor CN) bernilai <= 0, itu berarti belokannya 
    // searah jarum jam (Clockwise), sehingga sudut C membentuk cekungan (reflex angle).
    // Sudut cekung TIDAK BISA dipotong sebagai 'telinga' (ear).
    if (crossProduct(p, c, n) <= 0.0f) {
        return false;
    }

    // Cek apakah ada vertex polygon lain yang masuk ke dalam segitiga p-c-n
    for (size_t i = 0; i < polygon.size(); ++i) {
        if (i == prev || i == curr || i == next) continue;
        if (isPointInTriangle(vertices[polygon[i]], p, c, n)) {
            return false;
        }
    }
    return true;
}

void Mesh2D::processTriangulation() {
    faces.clear(); // Reset faces yang ada sebelumnya
    std::vector<unsigned int> loop = findClosedLoop();
    if (loop.size() < 3) return; // Belum ada bidang tertutup, return

    // [Calculus & Geometry] Menggunakan Rumus Shoelace (Shoelace Formula)
    // Rumus ini adalah bentuk diskrit dari Teorema Green (Green's Theorem) dalam Kalkulus.
    // Area = 0.5 * sum(x_i * y_{i+1} - x_{i+1} * y_i)
    // Di sini kita menghitung nilainya tanpa dikali 0.5 karena kita hanya peduli pada tanda (+ / -)
    // untuk menentukan orientasi bidang (searah atau berlawanan jarum jam).
    float area = 0;
    for (size_t i = 0; i < loop.size(); ++i) {
        const auto& v1 = vertices[loop[i]];
        const auto& v2 = vertices[loop[(i + 1) % loop.size()]];
        area += (v2.x - v1.x) * (v2.y + v1.y); 
    }
    
    // Pada sistem koordinat layar kartesius umum, area bernilai negatif artinya 
    // urutannya adalah Counter-Clockwise (CCW). Jika nilainya positif (Clockwise), 
    // kita harus membalik urutan titik-titiknya (reverse) agar konsisten dengan asumsi algoritma kita.
    if (area > 0) { 
        std::reverse(loop.begin(), loop.end());
    }

    // 2. Ear Clipping Algorithm untuk Triangulasi
    std::vector<unsigned int> remaining = loop;
    while (remaining.size() >= 3) {
        bool earFound = false;
        for (size_t i = 0; i < remaining.size(); ++i) {
            size_t prev = (i == 0) ? remaining.size() - 1 : i - 1;
            size_t next = (i + 1) % remaining.size();

            // Jika node saat ini adalah "telinga", potong dan jadikan wajah (face/segitiga) baru
            if (isEar(prev, i, next, remaining)) {
                faces.push_back({remaining[prev], remaining[i], remaining[next]});
                remaining.erase(remaining.begin() + i);
                earFound = true;
                break;
            }
        }
        
        // Safety guard agar tidak infinite loop jika polygon kompleks bersilangan
        if (!earFound) {
            break; 
        }
    }
}
