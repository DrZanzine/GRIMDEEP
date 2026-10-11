// =============================================================================
//  render/geometry.cpp — implémentation des primitives géométriques
// =============================================================================
#include "render/geometry.h"

#include "render/materials.h"
#include "render/model_loader.h"

void addGrid(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, int rows, int cols, float refSign,
             float mat, float seed) {
    uint32_t base = (uint32_t)m.v.size();
    std::vector<vec3> N(P.size(), vec3(0));
    auto id = [&](int r, int c) { return r * cols + (c % cols); };
    // Deux triangles par quad ; on accumule leurs normales (non normalisées : pondération par l'aire).
    for (int r = 0; r < rows - 1; r++)
        for (int c = 0; c < cols; c++) {
            int a = id(r, c), b = id(r, c + 1), cc = id(r + 1, c + 1), d = id(r + 1, c);
            vec3 n1 = glm::cross(P[b] - P[a], P[cc] - P[a]), n2 = glm::cross(P[cc] - P[a], P[d] - P[a]);
            N[a] += n1 + n2; N[b] += n1; N[cc] += n1 + n2; N[d] += n2;
            m.idx.push_back(base + a); m.idx.push_back(base + b); m.idx.push_back(base + cc);
            m.idx.push_back(base + a); m.idx.push_back(base + cc); m.idx.push_back(base + d);
        }
    for (size_t i = 0; i < P.size(); i++) {
        vec3 n = glm::normalize(N[i] + vec3(1e-6f));
        if (glm::dot(n, ref[i] - P[i]) * refSign < 0) n = -n;
        m.v.push_back({P[i], n, vec2(0), mat, seed});
    }
}

void addEllipsoid(Mesh& m, vec3 c, vec3 ax, vec3 ay, vec3 az, vec3 rad, float mat, float seed) {
    const int rows = 7, cols = 10;
    std::vector<vec3> P, ref;
    for (int r = 0; r < rows; r++) {
        float phi = PI * r / (rows - 1);  // latitude, du pôle +ay au pôle -ay
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols;  // longitude
            P.push_back(c + ax * (rad.x * std::sin(phi) * std::cos(a)) + ay * (rad.y * std::cos(phi)) + az * (rad.z * std::sin(phi) * std::sin(a)));
            ref.push_back(c);
        }
    }
    addGrid(m, P, ref, rows, cols, -1.0f, mat, seed);
}

void addBoardBox(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht, float mat, float seed,
                 Rng& rng, float jit) {
    // 8 coins : bit0 = signe sur dl, bit1 = signe sur dw, bit2 = signe sur dt
    vec3 corner[8];
    for (int k = 0; k < 8; k++) {
        float sx = (k & 1) ? 1.f : -1.f, sy = (k & 2) ? 1.f : -1.f, sz = (k & 4) ? 1.f : -1.f;
        corner[k] = c + dl * (sx * hl) + dw * (sy * hw) + dt * (sz * ht) + vec3(rng.r(-jit, jit), rng.r(-jit, jit), rng.r(-jit, jit));
    }
    vec2 uvOff(seed * 7.13f, seed * 3.71f);  // chaque planche montre une autre portion de la texture
    // 6 faces : pour chaque axe a, la face s = -1 puis la face s = +1
    for (int a = 0; a < 3; a++)
        for (int s = -1; s <= 1; s += 2) {
            int b = (a + 1) % 3, cc = (a + 2) % 3;
            const int cb[4] = {-1, 1, 1, -1}, ccs[4] = {-1, -1, 1, 1};
            vec3 p[4];
            vec2 uv[4];
            for (int q = 0; q < 4; q++) {
                int sg[3];
                sg[a] = s; sg[b] = cb[q]; sg[cc] = ccs[q];
                p[q] = corner[(sg[0] > 0 ? 1 : 0) | (sg[1] > 0 ? 2 : 0) | (sg[2] > 0 ? 4 : 0)];
                if (a == 0) uv[q] = vec2(sg[1] * hw, sg[2] * ht);
                else if (a == 1) uv[q] = vec2(sg[0] * hl, sg[2] * ht);
                else uv[q] = vec2(sg[0] * hl, sg[1] * hw);
                uv[q] += uvOff;
            }
            vec3 n = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]));
            vec3 fc = (p[0] + p[1] + p[2] + p[3]) * 0.25f;
            if (glm::dot(n, fc - c) < 0) n = -n;  // normale toujours vers l'extérieur
            uint32_t base = (uint32_t)m.v.size();
            for (int q = 0; q < 4; q++) m.v.push_back({p[q], n, uv[q], mat, seed});
            m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
}

void addBoard(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht, float mat, float seed, Rng& rng,
              float jit) {
    uint32_t i0 = (uint32_t)m.idx.size();
    if (m.geom == 1 && (mat == Mat::WOOD || mat == Mat::CHARRED_WOOD)) {
        // modèle de planche ; le bois calciné reprend le modèle en changeant le matériau du bois
        uint32_t v0 = (uint32_t)m.v.size();
        addPlankModel(m, c, dl, dt, hl, hw, ht, seed);
        if (mat != Mat::WOOD)
            for (size_t i = v0; i < m.v.size(); i++)
                if (m.v[i].mat == Mat::WOOD) m.v[i].mat = mat;
    } else if (m.geom != 2) {
        addBoardBox(m, c, dl, dw, dt, hl, hw, ht, mat, seed, rng, jit);
    }
    if (m.track) {
        Board b;
        b.c = c; b.dl = dl; b.dw = dw; b.dt = dt;
        b.hl = hl; b.hw = hw; b.ht = ht;
        b.i0 = i0;
        b.n = (uint32_t)m.idx.size() - i0;
        m.boards.push_back(b);
    }
}

bool addBoardBetween(Mesh& m, vec3 a, vec3 b, vec3 hint, float hw, float ht, float mat, Rng& rng, float extend,
                     float jit) {
    vec3 d = b - a;
    float len = glm::length(d);
    if (len < 1e-4f) return false;
    vec3 dl = d / len;
    // épaisseur : `hint` rendu perpendiculaire à la planche (avec des replis si dégénéré)
    vec3 t = hint - dl * glm::dot(hint, dl);
    if (glm::length(t) < 1e-3f) t = glm::cross(dl, vec3(0, 1, 0));
    if (glm::length(t) < 1e-3f) t = vec3(1, 0, 0);
    vec3 dt = glm::normalize(t), dw = glm::normalize(glm::cross(dt, dl));
    addBoard(m, (a + b) * 0.5f, dl, dw, dt, len * 0.5f + extend, hw, ht, mat, rng.f() * 10.0f, rng, jit);
    return true;
}

void addCylinder(Mesh& m, vec3 p0, vec3 p1, float r0, float r1, int seg, float mat, float seed, bool cap0, bool cap1) {
    vec3 ax = p1 - p0;
    float L = glm::length(ax);
    if (L < 1e-5f) return;
    ax /= L;
    vec3 t = glm::normalize(glm::cross(ax, std::fabs(ax.y) < 0.95f ? vec3(0, 1, 0) : vec3(1, 0, 0))), b = glm::cross(ax, t);
    float slope = (r0 - r1) / L;  // inclinaison des normales pour un cône
    uint32_t base = (uint32_t)m.v.size();
    for (int i = 0; i <= seg; i++) {
        float a = 2 * PI * i / seg;
        vec3 d = t * std::cos(a) + b * std::sin(a), n = glm::normalize(d + ax * slope);
        m.v.push_back({p0 + d * r0, n, vec2((float)i / seg, 0), mat, seed, 0});
        m.v.push_back({p1 + d * r1, n, vec2((float)i / seg, 1), mat, seed, 0});
    }
    for (int i = 0; i < seg; i++) {
        uint32_t a = base + 2 * i;
        m.idx.insert(m.idx.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
    }
    // disques de fermeture
    for (int c = 0; c < 2; c++) {
        float r = c ? r1 : r0;
        if ((c ? !cap1 : !cap0) || r < 1e-4f) continue;
        vec3 ctr = c ? p1 : p0, n = c ? ax : -ax;
        uint32_t cb = (uint32_t)m.v.size();
        m.v.push_back({ctr, n, vec2(0.5f), mat, seed, 0});
        for (int i = 0; i <= seg; i++) {
            float a = 2 * PI * i / seg;
            m.v.push_back({ctr + (t * std::cos(a) + b * std::sin(a)) * r, n, vec2(0.5f), mat, seed, 0});
        }
        for (int i = 0; i < seg; i++) m.idx.insert(m.idx.end(), {cb, cb + 1 + (uint32_t)i, cb + 2 + (uint32_t)i});
    }
}

void addSurface(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, const std::vector<vec2>& UV,
                const std::vector<float>& MT, int rows, int cols) {
    uint32_t base = (uint32_t)m.v.size();
    std::vector<vec3> N(P.size(), vec3(0));
    auto id = [&](int r, int c) { return (uint32_t)(r * cols + (c % cols)); };
    for (int r = 0; r < rows - 1; r++)
        for (int c = 0; c < cols; c++) {
            uint32_t a = id(r, c), b = id(r, c + 1), cc = id(r + 1, c + 1), d = id(r + 1, c);
            vec3 n1 = glm::cross(P[b] - P[a], P[cc] - P[a]), n2 = glm::cross(P[cc] - P[a], P[d] - P[a]);
            N[a] += n1 + n2; N[b] += n1; N[cc] += n1 + n2; N[d] += n2;
            m.idx.insert(m.idx.end(), {base + a, base + b, base + cc, base + a, base + cc, base + d});
        }
    for (size_t i = 0; i < P.size(); i++) {
        vec3 n = glm::normalize(N[i] + vec3(1e-7f));
        if (glm::dot(n, ref[i] - P[i]) > 0) n = -n;
        m.v.push_back({P[i], n, UV[i], MT[i], 0.0f, 0.0f});
    }
}

std::vector<vec3> catmullRomSpline(const std::vector<vec3>& cp, int per) {
    std::vector<vec3> out;
    int n = (int)cp.size();
    for (int i = 0; i < n - 1; i++) {
        vec3 p0 = cp[std::max(0, i - 1)], p1 = cp[i], p2 = cp[i + 1], p3 = cp[std::min(n - 1, i + 2)];
        for (int k = 0; k < per; k++) {
            float t = (float)k / per, t2 = t * t, t3 = t2 * t;
            out.push_back(0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
        }
    }
    out.push_back(cp.back());
    return out;
}

void addLoft(Mesh& m, const std::vector<vec3>& path, const std::function<float(float)>& rad, int seg, float mat,
             float flat) {
    int n = (int)path.size();
    if (n < 2) return;
    std::vector<vec3> P, ref;
    std::vector<vec2> UV;
    std::vector<float> MT;
    vec3 t0 = glm::normalize(path[1] - path[0] + vec3(1e-7f));
    vec3 X = glm::normalize(glm::cross(t0, std::fabs(t0.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    for (int i = 0; i < n; i++) {
        vec3 t = glm::normalize(path[std::min(n - 1, i + 1)] - path[std::max(0, i - 1)] + vec3(1e-7f));
        X = glm::normalize(X - t * glm::dot(X, t) + vec3(1e-7f));  // transport du repère (rotation minimale)
        vec3 Y = glm::cross(t, X);
        float u = (float)i / (n - 1), r = rad(u);
        for (int k = 0; k < seg; k++) {
            float a = 2 * PI * k / seg;
            P.push_back(path[i] + (X * (std::cos(a) * flat) + Y * std::sin(a)) * r);
            ref.push_back(path[i]);
            UV.push_back(vec2((float)k / seg, u));
            MT.push_back(mat);
        }
    }
    addSurface(m, P, ref, UV, MT, n, seg);
}

void addEllipsoidHiRes(Mesh& m, vec3 c, const mat3& B, vec3 r, float mat, int rows, int cols) {
    std::vector<vec3> P, ref;
    std::vector<vec2> UV;
    std::vector<float> MT;
    for (int i = 0; i < rows; i++) {
        float ph = PI * i / (rows - 1);
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols;
            P.push_back(c + B * vec3(r.x * std::sin(ph) * std::cos(a), r.y * std::cos(ph), r.z * std::sin(ph) * std::sin(a)));
            ref.push_back(c);
            UV.push_back(vec2((float)k / cols, (float)i / (rows - 1)));
            MT.push_back(mat);
        }
    }
    addSurface(m, P, ref, UV, MT, rows, cols);
}
