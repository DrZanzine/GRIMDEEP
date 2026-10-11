// =============================================================================
//  entities/enemy.cpp — animation et maillage des ennemis
// =============================================================================
#include "entities/enemy.h"

#include "core/random.h"
#include "render/geometry.h"
#include "render/materials.h"

const EnemyStat ENEMY_STATS[3] = {
    // vitesse PV dégâts délai portée éveil rayon centre recul sonné armement
    {2.4f, 3, 12, 1.2f, 1.7f, 13.0f, 0.55f, 0.9f, 7.5f, 0.45f, 0.42f},  // rôdeur
    {3.6f, 2, 8, 0.9f, 1.4f, 12.0f, 0.7f, 0.5f, 9.0f, 0.38f, 0.28f},    // araignée
    {1.5f, 6, 25, 1.9f, 2.5f, 14.0f, 1.0f, 1.6f, 4.0f, 0.22f, 0.72f}};  // golem de lave

const float ENEMY_SCALE[3] = {0.95f, 0.6f, 0.7f};

namespace {

/// Pose d'attaque d'un type : angle des bras levés / abattus, bascule du corps
/// (armement / frappe), recul pendant l'armement puis avancée pendant la fente.
struct AttackPose { float armUp, armSlam, pitchUp, pitchSlam, back, fwd; };
const AttackPose ATTACK_POSES[3] = {{-1.85f, 0.75f, -0.30f, 0.45f, 0.20f, 0.45f},
                                    {0.0f, 0.0f, -0.62f, 0.30f, 0.12f, 0.70f},
                                    {-2.7f, -0.4f, -0.28f, 0.32f, 0.18f, 0.35f}};

/// Ennemi à partir d'un modèle OBJ : placement + animation par groupes (rotation
/// autour des pivots du modèle). Renvoie faux si le modèle n'est pas chargé.
bool appendEnemyModel(Mesh& m, const Enemy& e, float time) {
    const ModelData& md = enemyModel(e.type);
    if (!md.ok) return false;
    const float sc = ENEMY_SCALE[e.type];
    vec3 up(0, 1, 0), f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), x = glm::cross(up, f);
    float a = e.anim, bob = e.type == 0 ? 0.03f * std::fabs(std::sin(a)) : e.type == 2 ? 0.04f * std::fabs(std::sin(a)) : 0.0f;
    // phases de l'attaque : armement 0..1, frappe 0..1, récupération 0..1
    float wu = 0, st = 0, rc = 0;
    if (e.atkT >= 0) {
        float W = ENEMY_STATS[e.type].wind, t = e.atkT;
        if (t < W) wu = t / W;
        else {
            wu = 1;
            if (t < W + ATK_STRIKE) st = (t - W) / ATK_STRIKE;
            else { st = 1; rc = std::min(1.0f, (t - W - ATK_STRIKE) / ATK_RECOVER); }
        }
    }
    const AttackPose& ac = ATTACK_POSES[e.type];
    float up_ = easeOut(wu), sl = easeIn(st), rec = easeOut(rc);
    // valeur d'un paramètre de pose : monte vers vUp, s'abat vers vSlam, revient à 0
    auto blend = [&](float vUp, float vSlam) {
        float v = vUp * up_;
        if (st > 0) v = mixf(vUp, vSlam, sl);
        if (rc > 0) v = vSlam * (1.0f - rec);
        return v;
    };
    bool atk = e.atkT >= 0;
    float h = e.hurtLean * e.hurtLean;
    float pitch = (e.type == 0 ? 0.07f : 0.0f) + (atk ? blend(ac.pitchUp, ac.pitchSlam) : 0.0f) - 0.55f * h;
    float fwd = (atk ? blend(-ac.back, ac.fwd) : 0.0f) - 0.14f * h, lift = e.type == 1 ? 0.28f * std::sin(PI * st) * (rc > 0 ? 0.0f : 1.0f) : 0.0f;
    float armA = atk ? blend(ac.armUp, ac.armSlam) : 0.0f;
    float armHurt = (e.type == 2 ? 0.7f : -0.9f) * h;
    float hip = (e.type == 0 ? 0.85f : e.type == 1 ? 0.3f : 1.35f);  // hauteur de la bascule du corps
    vec3 sh = (h > 0.01f) ? x * (std::sin(time * 70.0f) * 0.05f * h) : vec3(0);  // tremblement après un coup
    mat4 W = glm::translate(mat4(1.0f), e.pos - vec3(0, 1.65f, 0) + up * (bob + lift) + sh) * mat4(mat3(x, up, f)) * glm::translate(mat4(1.0f), vec3(0, 0, fwd))
           * glm::translate(mat4(1.0f), vec3(0, hip, 0)) * glm::rotate(mat4(1.0f), pitch, vec3(1, 0, 0)) * glm::translate(mat4(1.0f), vec3(0, -hip, 0)) * glm::scale(mat4(1.0f), vec3(sc));
    std::vector<mat4> G(md.gname.size(), mat4(1.0f));
    float walkK = atk ? 0.15f : 1.0f;
    auto swing = [&](size_t g, float ang, vec3 axis) {  // rotation du groupe g autour de son pivot
        vec3 p = md.gpivot[g];
        G[g] = glm::translate(mat4(1.0f), p) * glm::rotate(mat4(1.0f), ang, axis) * glm::translate(mat4(1.0f), -p);
    };
    for (size_t g = 0; g < md.gname.size(); g++) {
        const std::string& n = md.gname[g];
        float side = n.find("_p") != std::string::npos ? 1.0f : -1.0f;  // côté du membre (suffixe du nom de groupe)
        if (e.type == 1 && n.compare(0, 4, "leg_") == 0) {  // araignée : balayage + levée de chaque patte
            int k = n.back() - '0';
            float ph = a * 1.4f + (float)((k + (side > 0 ? 1 : 0)) & 1) * PI;
            float raise = (k == 0 && atk) ? blend(0.9f, 0.1f) : 0.0f;  // pattes avant dressées pendant l'attaque
            vec3 p = md.gpivot[g];
            G[g] = glm::translate(mat4(1.0f), vec3(0, std::max(0.0f, std::cos(ph)) * 0.12f * walkK, 0)) * glm::translate(mat4(1.0f), p)
                 * glm::rotate(mat4(1.0f), side * raise, vec3(0, 0, 1)) * glm::rotate(mat4(1.0f), std::sin(ph) * 0.22f * walkK, up) * glm::translate(mat4(1.0f), -p);
        } else if (n.compare(0, 4, "leg_") == 0) swing(g, side * std::sin(a) * (e.type == 0 ? 0.5f : 0.35f) * (atk ? 0.2f : 1.0f), vec3(1, 0, 0));
        else if (n.compare(0, 4, "arm_") == 0) swing(g, -side * std::sin(a) * (e.type == 0 ? 0.1f : 0.3f) * walkK + armA + armHurt, vec3(1, 0, 0));
    }
    size_t v0 = m.v.size();
    appendModel(m, md, W, 0.0f, vec2(0), &G);
    if (e.hurt > 0.2f)  // éclair au moment de l'impact
        for (size_t i = v0; i < m.v.size(); i++) m.v[i].mat = gCustomMats.flash;
    return true;
}

}  // namespace

const ModelData& enemyModel(int type) { return type == 0 ? gModels.mummy : type == 1 ? gModels.spider : gModels.golem; }

void appendEnemyMesh(Mesh& m, const Enemy& e, float time) {
    if (appendEnemyModel(m, e, time)) return;
    // --- formes de secours (modèle absent) : repère f (avant), up, rt (droite) aux pieds
    vec3 up(0, 1, 0);
    vec3 f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), rt = glm::normalize(glm::cross(f, up));
    vec3 feet = e.pos - vec3(0, 1.65f, 0);
    float a = e.anim;
    Rng z(1);
    auto E = [&](vec3 c, vec3 rad, float mat, float seed = 0.0f) { addEllipsoid(m, feet + f * c.x + up * c.y + rt * c.z, f, up, rt, rad, mat, seed); };
    auto P = [&](vec3 c) { return feet + f * c.x + up * c.y + rt * c.z; };
    if (e.type == 0) {  // rôdeur : silhouette voûtée aux yeux jaunes
        for (int s = -1; s <= 1; s += 2) {
            float sw = std::sin(a + (s > 0 ? 0.0f : PI)) * 0.25f;
            E(vec3(sw * 0.5f, 0.45f, s * 0.13f), vec3(0.09f, 0.45f, 0.09f), Mat::DARK_CLOTH);
            E(vec3(0.25f - sw * 0.3f, 1.05f, s * 0.36f), vec3(0.09f, 0.4f, 0.07f), Mat::DARK_CLOTH);
            E(vec3(0.45f, 0.95f, s * 0.34f), vec3(0.3f, 0.05f, 0.05f), Mat::DARK_CLOTH);
            E(vec3(0.34f, 1.67f, s * 0.07f), vec3(0.03f, 0.025f, 0.025f), Mat::YELLOW_GLOW);
        }
        E(vec3(0.08f, 1.15f, 0), vec3(0.2f, 0.42f, 0.28f), Mat::DARK_CLOTH);
        E(vec3(0.22f, 1.62f, 0), vec3(0.15f, 0.17f, 0.14f), Mat::DARK_CLOTH);
    } else if (e.type == 1) {  // araignée géante
        E(vec3(0, 0.55f, 0), vec3(0.34f, 0.24f, 0.28f), Mat::CHITIN);
        E(vec3(0.34f, 0.5f, 0), vec3(0.17f, 0.14f, 0.15f), Mat::CHITIN);
        for (int s = -1; s <= 1; s += 2) {
            E(vec3(0.47f, 0.56f, s * 0.05f), vec3(0.03f), Mat::YELLOW_GLOW);
            E(vec3(0.45f, 0.5f, s * 0.1f), vec3(0.025f), Mat::YELLOW_GLOW);
        }
        for (int k = 0; k < 8; k++) {
            float side = (k & 1) ? 1.0f : -1.0f;
            int idx = k >> 1;
            float ph = a * 1.4f + k * 1.3f;
            vec3 hip(0.2f - 0.13f * idx, 0.55f, side * 0.22f), knee(0.3f - 0.14f * idx + std::sin(ph) * 0.05f, 1.0f, side * 0.6f);
            vec3 foot(0.3f - 0.18f * idx + std::sin(ph) * 0.14f, 0.02f + std::max(0.0f, std::cos(ph)) * 0.08f, side * 1.0f);
            addBoardBetween(m, P(hip), P(knee), up, 0.022f, 0.022f, Mat::CHITIN, z, 0, 0);
            addBoardBetween(m, P(knee), P(foot), up, 0.018f, 0.018f, Mat::CHITIN, z, 0, 0);
        }
    } else {  // golem de lave
        for (int s = -1; s <= 1; s += 2) {
            float sw = std::sin(a + (s > 0 ? 0.0f : PI)) * 0.3f;
            addBoardBetween(m, P(vec3(sw * 0.4f, 0.0f, s * 0.38f)), P(vec3(sw * 0.2f, 1.1f, s * 0.38f)), up, 0.22f, 0.22f, Mat::GOLEM, z, 0, 0);
            addBoardBetween(m, P(vec3(0.1f, 2.25f, s * 0.78f)), P(vec3(0.5f - sw * 0.5f, 1.05f, s * 0.95f)), up, 0.2f, 0.2f, Mat::GOLEM, z, 0, 0);
            E(vec3(0.4f, 2.72f, s * 0.12f), vec3(0.06f), Mat::YELLOW_GLOW);
        }
        E(vec3(0, 1.75f, 0), vec3(0.5f, 0.75f, 0.65f), Mat::GOLEM);
        E(vec3(0.1f, 2.65f, 0), vec3(0.3f, 0.3f, 0.3f), Mat::GOLEM);
    }
}
