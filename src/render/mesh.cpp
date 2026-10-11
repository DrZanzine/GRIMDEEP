// =============================================================================
//  render/mesh.cpp — gestion des tampons OpenGL d'un maillage
// =============================================================================
#include "render/mesh.h"

void Mesh::endChunk(size_t v0, size_t v1) {
    if (idx.size() == cIdx0 || v1 <= v0) return;
    vec3 mn(1e9f), mx(-1e9f);
    for (size_t i = v0; i < v1; i++) {
        mn = glm::min(mn, v[i].p);
        mx = glm::max(mx, v[i].p);
    }
    Chunk c;
    c.first = (uint32_t)cIdx0;
    c.count = (uint32_t)(idx.size() - cIdx0);
    c.c = (mn + mx) * 0.5f;
    c.r = glm::length(mx - mn) * 0.5f;
    chunks.push_back(c);
}

void Mesh::ensure() {
    if (vao) return;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    // attributs : 0 position, 1 normale, 2 uv, 3 (matériau, graine), 4 biome
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)32);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)40);
}

void Mesh::upload(GLenum usage) {
    ensure();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), usage);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), usage);
}

void Mesh::clearCpu() {
    v.clear();
    idx.clear();
    chunks.clear();
    boards.clear();
}

void Mesh::killIndices(uint32_t i0, uint32_t n) {
    for (uint32_t k = 0; k < n; k++) idx[i0 + k] = 0;
    std::vector<uint32_t> z(n, 0);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)i0 * 4, n * 4, z.data());
}

void Mesh::destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
}

void Mesh::draw(const std::function<bool(const Chunk&)>& keep) const {
    if (!vao) return;
    glBindVertexArray(vao);
    for (const Chunk& c : chunks)
        if (keep(c)) glDrawElements(GL_TRIANGLES, c.count, GL_UNSIGNED_INT, (void*)((size_t)c.first * 4));
}

void Mesh::drawAll() const {
    if (!vao || idx.empty()) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, nullptr);
}
