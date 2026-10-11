// =============================================================================
//  entities/lady.cpp — construction procédurale de « la Dame »
// =============================================================================
#include "entities/lady.h"

#include <cstdio>

#include "core/noise.h"
#include "core/random.h"
#include "render/geometry.h"
#include "render/materials.h"

void buildLady(LadyModel& lady) {
    Mesh& m = lady.mesh;
    Rng hr(1313);
    const mat3 I3(1.0f);
    // silhouette (demi-largeurs rx, rz en fonction de la hauteur y) : robe évasée, taille fine, côtes, épaules osseuses
    auto prof = [](float y, float& rx, float& rz) {
        if (y < 1.42f) { float t = std::pow(y / 1.42f, 0.6f); rx = mixf(0.50f, 0.15f, t); rz = mixf(0.42f, 0.11f, t); }
        else if (y < 1.88f) { float t = smoothStep(1.42f, 1.88f, y); rx = mixf(0.15f, 0.18f, t); rz = mixf(0.11f, 0.125f, t); }
        else if (y < 2.01f) { float t = (y - 1.88f) / 0.13f; rx = mixf(0.18f, 0.24f, std::sqrt(t)); rz = mixf(0.125f, 0.10f, t); }
        else { float t = std::min(1.0f, (y - 2.01f) / 0.09f); rx = mixf(0.24f, 0.055f, std::sqrt(t)); rz = mixf(0.10f, 0.05f, std::sqrt(t)); }
    };
    auto bodyZ = [](float y) { return 0.05f * smoothStep(1.55f, 2.1f, y); };  // dos voûté, épaules jetées en avant

    // ---- corps + robe : surface de révolution déformée (plis de la robe, côtes, clavicules)
    {
        const int rows = 120, cols = 80;
        std::vector<vec3> P, ref;
        std::vector<vec2> UV;
        std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float y = 0.02f + 2.08f * (float)i / (rows - 1), rx, rz;
            prof(y, rx, rz);
            float zc = bodyZ(y);
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols, c = std::cos(th), sn = std::sin(th), f = 1.0f;
                if (y < 1.5f) {  // plis de la robe, plus marqués vers le bas
                    float w = std::pow(1.0f - y / 1.5f, 1.3f);
                    f += w * (0.075f * std::sin(9 * th + 1.7f * std::sin(3 * th + y * 2.0f)) + 0.035f * std::sin(19 * th + y * 7.0f)) +
                         0.05f * w * (fbm3D(vec3(c * 2.0f, y * 3.0f, sn * 2.0f), 3, 901) - 0.5f);
                }
                float neck = 1.86f - 0.13f * std::pow(std::max(0.0f, sn), 2.0f);  // décolleté
                bool skin = y > neck;
                if (skin && sn > 0.2f) f += 0.035f * std::pow(std::fabs(std::sin(y * 55.0f)), 6.0f) * sn;  // côtes / clavicules saillantes
                P.push_back(vec3(c * rx * f, y, zc + sn * rz * f));
                ref.push_back(vec3(0, y, zc));
                UV.push_back(vec2(th / (2 * PI), y));
                MT.push_back(skin ? Mat::LADY_SKIN : Mat::LADY_DRESS);
            }
        }
        addSurface(m, P, ref, UV, MT, rows, cols);
    }
    for (int sd = -1; sd <= 1; sd += 2) {  // jambes maigres et pieds nus sous la robe
        float s = (float)sd;
        addLoft(m, catmullRomSpline({vec3(s * 0.09f, 1.05f, 0.0f), vec3(s * 0.10f, 0.55f, 0.02f), vec3(s * 0.10f, 0.09f, 0.0f)}, 10),
                [](float u) { return mixf(0.06f, 0.03f, u); }, 20, Mat::LADY_SKIN);
        addEllipsoidHiRes(m, vec3(s * 0.10f, 0.035f, 0.07f), I3, vec3(0.04f, 0.03f, 0.105f), Mat::LADY_SKIN, 12, 18);
    }
    addLoft(m, catmullRomSpline({vec3(0, 2.02f, 0.03f), vec3(0, 2.18f, 0.05f), vec3(0, 2.33f, 0.075f)}, 10), [](float u) { return mixf(0.05f, 0.043f, u); },
            32, Mat::LADY_SKIN, 0.88f);  // long cou

    // ---- tête penchée : crâne allongé, mâchoire étirée, orbites creuses, bouche béante
    mat4 Hm = glm::translate(mat4(1.0f), vec3(0, 2.40f, 0.075f)) * glm::rotate(mat4(1.0f), 0.32f, vec3(0, 0, 1)) * glm::rotate(mat4(1.0f), 0.12f, vec3(1, 0, 0));
    mat3 H3(Hm);
    auto HP = [&](vec3 l) { return vec3(Hm * vec4(l, 1.0f)); };  // repère de la tête -> repère de la Dame
    const vec3 eyeL[2] = {vec3(-0.036f, 0.012f, 0.083f), vec3(0.036f, 0.012f, 0.083f)};
    {
        const int rows = 56, cols = 64;
        std::vector<vec3> P, ref;
        std::vector<vec2> UV;
        std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float y = mixf(-0.215f, 0.145f, (float)i / (rows - 1)), rx, rz, zc = 0.0f;
            if (y >= -0.03f) {  // calotte
                float e = (y + 0.03f) / 0.175f, q = std::sqrt(std::max(0.0f, 1.0f - e * e));
                rx = 0.097f * q;
                rz = 0.112f * q;
            } else {  // mâchoire étirée
                float e = (-0.03f - y) / 0.185f;
                rx = mixf(0.097f, 0.012f, std::pow(e, 1.15f));
                rz = mixf(0.112f, 0.022f, std::pow(e, 1.1f));
                zc = 0.012f * e;
            }
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols, c = std::cos(th), sn = std::sin(th);
                vec3 p(c * rx, y, zc + sn * rz * (sn > 0 ? 0.93f : 1.0f)), rd = glm::normalize(vec3(c, 0.0f, sn) + vec3(1e-6f));
                for (const vec3& e : eyeL) {  // orbites creuses
                    float d = glm::length(p - vec3(e.x, e.y, e.z - 0.01f));
                    p -= rd * 0.024f * std::exp(-(d / 0.03f) * (d / 0.03f));
                }
                for (int sd = -1; sd <= 1; sd += 2) {  // joues creusées
                    float d = glm::length(p - vec3(sd * 0.055f, -0.07f, 0.07f));
                    p -= rd * 0.014f * std::exp(-(d / 0.035f) * (d / 0.035f));
                }
                {  // bouche
                    float dx = p.x / 0.022f, dy = (p.y + 0.11f) / 0.055f;
                    if (p.z > 0) p -= rd * 0.02f * std::exp(-(dx * dx + dy * dy));
                }
                P.push_back(HP(p));
                ref.push_back(HP(vec3(0, y, zc)));
                UV.push_back(vec2(th / (2 * PI), (float)i / (rows - 1)));
                MT.push_back(Mat::LADY_SKIN);
            }
        }
        addSurface(m, P, ref, UV, MT, rows, cols);
    }
    for (int e = 0; e < 2; e++) {
        addEllipsoidHiRes(m, HP(eyeL[e] - vec3(0, 0, 0.007f)), H3, vec3(0.024f, 0.017f, 0.012f), Mat::LADY_DARK, 12, 18);  // orbite noire
        addEllipsoidHiRes(m, HP(eyeL[e]), H3, vec3(0.0125f), Mat::LADY_EYE, 12, 16);                                       // œil rouge
        lady.eye[e] = HP(eyeL[e] + vec3(0, 0, 0.01f));
    }
    addEllipsoidHiRes(m, HP(vec3(0, -0.115f, 0.072f)), H3, vec3(0.017f, 0.05f, 0.014f), Mat::LADY_DARK, 14, 18);  // bouche béante
    const vec3 head = HP(vec3(0));
    lady.head = head;
    {  // calotte de cheveux (rentrée sous la peau devant le visage)
        const int rows = 30, cols = 56;
        std::vector<vec3> P, ref;
        std::vector<vec2> UV;
        std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float ph = 1.95f * i / (rows - 1);
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols;
                vec3 p(0.104f * std::sin(ph) * std::cos(th), 0.152f * std::cos(ph) + 0.005f, 0.122f * std::sin(ph) * std::sin(th) - 0.012f);
                if (std::sin(th) > 0.2f && ph > 0.62f) p *= 0.82f;
                P.push_back(HP(p));
                ref.push_back(HP(vec3(0, 0.005f, -0.012f)));
                UV.push_back(vec2(0));
                MT.push_back(Mat::LADY_HAIR);
            }
        }
        addSurface(m, P, ref, UV, MT, rows, cols);
    }
    for (int n = 0; n < 230; n++) {  // longs cheveux noirs, mèches qui tombent jusqu'aux hanches
        float ph = hr.r(0.05f, 1.9f), th = hr.r(0.0f, 2 * PI), sn = std::sin(th), cs = std::cos(th);
        if (sn > 0.3f && ph > 0.55f && std::fabs(cs) < 0.6f) continue;  // le visage reste dégagé
        vec3 rl(0.104f * std::sin(ph) * cs, 0.152f * std::cos(ph) + 0.005f, 0.122f * std::sin(ph) * sn - 0.012f);
        vec3 root = HP(rl), out = glm::normalize(H3 * rl);
        float L = sn < 0.0f ? hr.r(0.95f, 1.5f) : hr.r(0.6f, 1.1f);
        std::vector<vec3> pts{root};
        vec3 p = root;
        for (int j = 1; j <= 10; j++) {
            float f = (float)j / 10;
            p += out * (0.03f * (1.0f - f)) + vec3(hr.r(-0.014f, 0.014f), -L / 10, hr.r(-0.014f, 0.014f));
            if (p.y < 2.12f) {  // la mèche glisse sur le corps sans le traverser
                float rx, rz;
                prof(std::max(0.05f, p.y), rx, rz);
                float zc = bodyZ(p.y), dx = p.x / (rx + 0.03f), dz = (p.z - zc) / (rz + 0.03f), e2 = dx * dx + dz * dz;
                if (e2 < 1.0f) {
                    float k = 1.0f / std::sqrt(std::max(e2, 1e-4f));
                    p.x *= k;
                    p.z = zc + (p.z - zc) * k;
                }
            }
            vec3 hl = p - head;  // ni la tête
            if (glm::length(hl * vec3(1.0f / 0.11f, 1.0f / 0.16f, 1.0f / 0.13f)) < 1.0f) p = head + hl * 1.08f;
            pts.push_back(p);
        }
        addLoft(m, catmullRomSpline(pts, 3), [](float u) { return mixf(0.0065f, 0.0022f, u); }, 5, Mat::LADY_HAIR);
    }
    // ---- bras trop longs, mains crochues aux doigts interminables
    for (int sd = -1; sd <= 1; sd += 2) {
        float s = (float)sd;
        vec3 W(s * 0.31f, 0.80f, 0.12f);  // poignet
        std::vector<vec3> arm = catmullRomSpline({vec3(s * 0.215f, 2.0f, 0.02f), vec3(s * 0.265f, 1.74f, 0.03f), vec3(s * 0.29f, 1.40f, 0.05f), vec3(s * 0.30f, 1.08f, 0.09f), W}, 10);
        addLoft(m, arm,
                [](float u) { return u < 0.5f ? mixf(0.047f, 0.029f, u / 0.5f) : u < 0.7f ? mixf(0.029f, 0.034f, (u - 0.5f) / 0.2f) : mixf(0.034f, 0.022f, (u - 0.7f) / 0.3f); },
                24, Mat::LADY_SKIN);
        addEllipsoidHiRes(m, vec3(s * 0.29f, 1.40f, 0.05f), I3, vec3(0.033f), Mat::LADY_SKIN, 10, 14);  // coude osseux
        vec3 K = W + vec3(s * 0.008f, -0.115f, 0.02f);  // jointures
        addLoft(m, catmullRomSpline({W, W + vec3(s * 0.005f, -0.06f, 0.012f), K}, 4), [](float u) { return mixf(0.024f, 0.028f, u); }, 16, Mat::LADY_SKIN, 0.55f);
        for (int k = 0; k < 4; k++) {  // doigts
            float off = (k - 1.5f) * 0.0135f, len = (k == 1 || k == 2) ? 0.1f : 0.085f;
            vec3 b = K + vec3(0, 0, off);
            vec3 j1 = b + vec3(0, -len, 0.004f), j2 = j1 + vec3(s * 0.003f, -len * 0.85f, 0.016f), j3 = j2 + vec3(0, -len * 0.7f, 0.026f);
            addLoft(m, catmullRomSpline({b, j1, j2, j3}, 5), [](float u) { return mixf(0.0085f, 0.0052f, u); }, 10, Mat::LADY_SKIN);
            addEllipsoidHiRes(m, j3 + vec3(0, -0.006f, 0.003f), I3, vec3(0.0055f, 0.012f, 0.0055f), Mat::LADY_DARK, 8, 10);  // ongle noir
        }
        vec3 tb = W + vec3(-s * 0.012f, -0.05f, 0.03f), t1 = tb + vec3(-s * 0.004f, -0.055f, 0.02f), t2 = t1 + vec3(0, -0.05f, 0.022f);  // pouce
        addLoft(m, catmullRomSpline({tb, t1, t2}, 5), [](float u) { return mixf(0.01f, 0.006f, u); }, 10, Mat::LADY_SKIN);
        addEllipsoidHiRes(m, t2 + vec3(0, -0.005f, 0.003f), I3, vec3(0.006f, 0.011f, 0.006f), Mat::LADY_DARK, 8, 10);
    }
    std::printf("La Dame : %zu sommets, %zu triangles\n", m.v.size(), m.idx.size() / 3);
}
