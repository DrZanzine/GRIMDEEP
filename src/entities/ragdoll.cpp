// =============================================================================
//  entities/ragdoll.cpp — simulation physique des corps
// =============================================================================
#include "entities/ragdoll.h"

#include "world/world.h"

namespace {

/// Gabarit calculé une fois par type à partir du modèle : pivots et centres des
/// membres, boîte englobante du tronc et ses points de contact.
struct RagdollTemplate {
    bool ok = false;
    vec3 cM = vec3(0), bmin = vec3(0), bmax = vec3(0);  ///< centre et boîte du tronc (groupe 0)
    std::vector<vec3> pivot, cent;                       ///< rotule et centre de chaque groupe
    std::vector<float> len;                              ///< distance rotule -> centre
    std::vector<vec3> pts;                               ///< points de contact (8 coins + haut + bas)
};
RagdollTemplate gTemplates[3];

/// Calcule (si besoin) le gabarit du type `type`.
void buildTemplate(int type) {
    const ModelData& md = enemyModel(type);
    RagdollTemplate& t = gTemplates[type];
    if (t.ok || !md.ok) return;
    size_t ng = md.gname.size();
    t.pivot = md.gpivot;
    t.cent.assign(ng, vec3(0));
    t.len.assign(ng, 0.0f);
    std::vector<int> cnt(ng, 0);
    t.bmin = vec3(1e9f);
    t.bmax = vec3(-1e9f);
    for (size_t i = 0; i < md.v.size(); i++) {
        int g = md.vg[i];
        t.cent[g] += md.v[i].p;
        cnt[g]++;
        if (g == 0) { t.bmin = glm::min(t.bmin, md.v[i].p); t.bmax = glm::max(t.bmax, md.v[i].p); }
    }
    for (size_t g = 0; g < ng; g++) {
        if (cnt[g]) t.cent[g] /= (float)cnt[g];
        t.len[g] = glm::length(t.cent[g] - t.pivot[g]);
    }
    t.cM = (t.bmin + t.bmax) * 0.5f;
    for (int k = 0; k < 8; k++) t.pts.push_back(vec3((k & 1) ? t.bmax.x : t.bmin.x, (k & 2) ? t.bmax.y : t.bmin.y, (k & 4) ? t.bmax.z : t.bmin.z));
    t.pts.push_back(vec3(t.cM.x, t.bmax.y, t.cM.z));
    t.pts.push_back(vec3(t.cM.x, t.bmin.y, t.cM.z));
    t.ok = true;
}

}  // namespace

Ragdoll makeRagdoll(const Enemy& e, vec3 dir, float knock, Rng& rng) {
    Ragdoll r;
    r.type = e.type;
    buildTemplate(e.type);
    const RagdollTemplate& T = gTemplates[e.type];
    r.sc = ENEMY_SCALE[e.type];
    vec3 up(0, 1, 0), f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), x = glm::cross(up, f);
    r.R = mat3(x, up, f);
    r.hints = e.hints;
    r.C = e.pos - vec3(0, 1.65f, 0) + r.R * (r.sc * T.cM);
    r.v = dir * (knock * 0.85f + 1.0f) + vec3(0, 1.3f, 0);  // projeté en arrière par l'impact
    r.w = glm::cross(up, dir) * rng.r(2.4f, 3.4f);          // bascule vers l'arrière
    for (size_t g = 1; g < T.pivot.size(); g++) {
        Ragdoll::Limb l;
        l.g = (int)g;
        l.L = std::max(0.14f, r.sc * T.len[g]);
        vec3 d = T.cent[g] - T.pivot[g];
        l.restL = glm::length(d) > 1e-4f ? glm::normalize(d) : vec3(0, -1, 0);
        l.tip = r.C + r.R * (r.sc * (T.cent[g] - T.cM));
        l.prev = l.tip - r.v * 0.016f;
        r.limbs.push_back(l);
    }
    return r;
}

void stepRagdoll(World& world, Ragdoll& r, float dt) {
    buildTemplate(r.type);
    const RagdollTemplate& T = gTemplates[r.type];
    if (!T.ok) return;
    r.t += dt;
    int sub = std::max(1, (int)std::ceil(dt / 0.012f));
    float h = dt / sub;
    vec3 up(0, 1, 0);
    auto floorAt = [&](vec3 p) { return world.constrain(vec3(p.x, p.y, p.z), false, r.hints).y - 1.65f; };  // altitude du sol sous p
    // inverse du moment d'inertie (approximation : sphère de la taille du tronc)
    const float invI = 1.0f / (0.45f * std::pow(r.sc * (T.bmax.y - T.bmin.y) * 0.5f + 0.15f, 2.0f));
    for (int s = 0; s < sub; s++) {
        if (!r.asleep) {
            r.v.y -= 9.8f * h;
            r.C += r.v * h;
            float wl = glm::length(r.w);
            if (wl > 1e-4f) r.R = mat3(glm::rotate(mat4(1.0f), wl * h, r.w / wl)) * r.R;
            // ré-orthonormalisation de l'orientation (évite la dérive numérique)
            vec3 c0 = glm::normalize(r.R[0]), c1 = glm::normalize(r.R[1] - c0 * glm::dot(r.R[1], c0)), c2 = glm::cross(c0, c1);
            r.R = mat3(c0, c1, c2);
            // parois de la galerie / blocs de pierre
            {
                vec3 q = world.constrain(r.C, false, r.hints);
                vec3 d(q.x - r.C.x, 0, q.z - r.C.z);
                if (glm::length(d) > 1e-5f) {
                    vec3 n = glm::normalize(d);
                    r.C.x = q.x;
                    r.C.z = q.z;
                    float vn = glm::dot(r.v, n);
                    if (vn < 0) r.v -= n * (vn * 1.4f);
                }
                vec3 pe(r.C.x, q.y, r.C.z), p0 = pe;
                world.collideBoards(pe, false);
                vec3 dd = pe - p0;
                dd.y = 0;
                if (glm::length(dd) > 1e-5f) {
                    r.C.x = pe.x;
                    r.C.z = pe.z;
                    vec3 n = glm::normalize(dd);
                    float vn = glm::dot(r.v, n);
                    if (vn < 0) r.v -= n * (vn * 1.4f);
                }
            }
            // sol : impulsions aux points de contact (rebond amorti + frottement)
            float maxPen = 0;
            bool touch = false;
            for (const vec3& pl : T.pts) {
                vec3 rr = r.R * (r.sc * (pl - T.cM)), p = r.C + rr;
                float pen = floorAt(p) + 0.03f - p.y;
                if (pen <= 0) continue;
                maxPen = std::max(maxPen, pen);
                touch = true;
                vec3 vp = r.v + glm::cross(r.w, rr);  // vitesse du point de contact
                if (vp.y < 0) {
                    vec3 rn = glm::cross(rr, up);
                    float jn = -(1.0f + 0.22f) * vp.y / (1.0f + glm::dot(rn, rn) * invI);  // impulsion normale (restitution 0,22)
                    r.v += up * jn;
                    r.w += glm::cross(rr, up * jn) * invI;
                    vec3 vt = vp - up * vp.y;
                    float vtl = glm::length(vt);
                    if (vtl > 1e-4f) {  // frottement de Coulomb (coefficient 0,55)
                        vec3 td = -vt / vtl;
                        float jt = std::min(0.55f * jn, vtl / (1.0f + 0.5f * glm::dot(rn, rn) * invI));
                        r.v += td * jt;
                        r.w += glm::cross(rr, td * jt) * invI;
                    }
                }
            }
            r.C.y += maxPen * 0.85f;
            r.w *= std::max(0.0f, 1.0f - (touch ? 1.6f : 0.15f) * h);
            r.v.x *= std::max(0.0f, 1.0f - (touch ? 1.4f : 0.0f) * h);
            r.v.z *= std::max(0.0f, 1.0f - (touch ? 1.4f : 0.0f) * h);
            if (touch && glm::length(r.v) < 0.35f && glm::length(r.w) < 0.7f) r.rest += h;
            else r.rest = 0;
            if (r.rest > 0.5f) { r.asleep = true; r.v = vec3(0); r.w = vec3(0); }
        }
        // membres : pendules de Verlet accrochés à leur rotule
        for (auto& l : r.limbs) {
            vec3 Pw = r.C + r.R * (r.sc * (T.pivot[l.g] - T.cM));
            vec3 vel = (l.tip - l.prev) * 0.987f;
            l.prev = l.tip;
            l.tip += vel + vec3(0, -9.8f * h * h, 0);
            vec3 d = l.tip - Pw;
            float len = glm::length(d);
            if (len > 1e-4f) l.tip = Pw + d * (l.L / len);  // longueur constante
            // limite articulaire : au plus 120° par rapport à la direction de repos
            vec3 rw = glm::normalize(r.R * l.restL), dn = glm::normalize(l.tip - Pw);
            float cs = glm::dot(rw, dn);
            if (cs < -0.5f) {
                vec3 pr = dn - rw * cs;
                float pl = glm::length(pr);
                if (pl > 1e-4f) { dn = rw * -0.5f + (pr / pl) * 0.866f; l.tip = Pw + dn * l.L; }
            }
            float fl = floorAt(l.tip) + 0.07f;
            if (l.tip.y < fl) {  // traîne au sol
                l.tip.y = fl;
                l.prev.y = fl;
                l.prev.x += (l.tip.x - l.prev.x) * 0.45f;
                l.prev.z += (l.tip.z - l.prev.z) * 0.45f;
                d = l.tip - Pw;
                len = glm::length(d);
                if (len > 1e-4f) l.tip = Pw + d * (l.L / len);
            }
        }
    }
}

void appendRagdollMesh(Mesh& m, const Ragdoll& r, float k) {
    const ModelData& md = enemyModel(r.type);
    const RagdollTemplate& T = gTemplates[r.type];
    if (!md.ok || !T.ok) return;
    std::vector<mat4> G(md.gname.size(), mat4(1.0f));
    mat4 Rb(r.R);
    float sc = r.sc * k;
    G[0] = glm::translate(mat4(1.0f), r.C) * Rb * glm::scale(mat4(1.0f), vec3(sc)) * glm::translate(mat4(1.0f), -T.cM);
    for (const auto& l : r.limbs) {
        // rotation qui amène la direction de repos du membre sur sa direction actuelle
        vec3 Pw = r.C + r.R * (sc * (T.pivot[l.g] - T.cM)), rw = glm::normalize(r.R * l.restL), dn = glm::normalize(l.tip - Pw);
        float c = std::min(1.0f, std::max(-1.0f, glm::dot(rw, dn)));
        vec3 ax = glm::cross(rw, dn);
        float al = glm::length(ax);
        if (al < 1e-5f) ax = glm::cross(rw, std::fabs(rw.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0));
        ax = glm::normalize(ax);
        mat4 Ra = glm::rotate(mat4(1.0f), std::acos(c), ax);
        G[l.g] = glm::translate(mat4(1.0f), Pw) * Ra * Rb * glm::scale(mat4(1.0f), vec3(sc)) * glm::translate(mat4(1.0f), -T.pivot[l.g]);
    }
    appendModel(m, md, mat4(1.0f), 0.0f, vec2(0), &G);
}
