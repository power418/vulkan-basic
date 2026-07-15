#pragma once

#include <vector>

struct Vertex2D {
    float x;
    float y;
};

struct Edge {
    unsigned int v1;
    unsigned int v2;
};

struct Face {
    unsigned int v1;
    unsigned int v2;
    unsigned int v3;
};

class Mesh2D {
public:
    Mesh2D() = default;
    ~Mesh2D() = default;

    // Menambahkan vertex, mengembalikan index dari vertex tersebut
    unsigned int addVertex(float x, float y);
    
    // Menambahkan edge (garis). 
    // Fungsi ini akan otomatis mengecek apakah terbentuk bidang (polygon) tertutup.
    // Jika ya, akan dilakukan triangulasi otomatis (generate mesh).
    void addEdge(unsigned int v1, unsigned int v2);

    const std::vector<Vertex2D>& getVertices() const;
    const std::vector<Edge>& getEdges() const;
    const std::vector<Face>& getFaces() const; // Mengembalikan mesh / segitiga

    void clear();

    // Memecah segitiga yang sudah ada untuk memasukkan titik interior ke dalam mesh
    bool insertInteriorPoint(unsigned int vIndex);

    // Properti untuk warna dan opacity area/bidang
    void setOpacity(float o);
    float getOpacity() const;

    // Untuk warna (R, G, B)
    void setColor(float r, float g, float b);
    void getColor(float& r, float& g, float& b) const;

private:
    float opacity = 1.0f; // Default 100% solid
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;

    std::vector<Vertex2D> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;

    // Fungsi internal untuk memproses triangulasi dengan Ear Clipping Algorithm
    void processTriangulation();
    
    // Mencari jalur tertutup (loop/polygon) dari edge-edge yang ada
    std::vector<unsigned int> findClosedLoop();

    // Helper functions matematika 2D
    float crossProduct(const Vertex2D& a, const Vertex2D& b, const Vertex2D& c) const;
    bool isPointInTriangle(const Vertex2D& p, const Vertex2D& a, const Vertex2D& b, const Vertex2D& c) const;
    bool isEar(size_t prev, size_t curr, size_t next, const std::vector<unsigned int>& polygon) const;
};
