// =============================================================================
//  entities/props.cpp — géométrie des objets construits en code
// =============================================================================
#include "entities/props.h"

#include "core/random.h"
#include "render/geometry.h"
#include "render/materials.h"

void buildPickaxe(Mesh& m) {
    Rng r(5);
    vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    const float TOP = 0.56f;  // hauteur de la tête
    addCylinder(m, vec3(0, -0.15f, 0), vec3(0, TOP + 0.02f, 0), 0.027f, 0.021f, 10, Mat::WOOD, 0.3f);              // manche
    addCylinder(m, vec3(0, -0.145f, 0), vec3(0, 0.19f, 0), 0.0305f, 0.0305f, 10, Mat::DARK_CLOTH, 0.0f);            // poignée de cuir
    addCylinder(m, vec3(0, -0.165f, 0), vec3(0, -0.145f, 0), 0.036f, 0.036f, 10, Mat::STEEL, 0.0f);                 // pommeau
    addCylinder(m, vec3(0, 0.19f, 0), vec3(0, 0.205f, 0), 0.035f, 0.035f, 10, Mat::STEEL, 0.0f);                    // virole
    addCylinder(m, vec3(0, TOP - 0.085f, 0), vec3(0, TOP - 0.04f, 0), 0.033f, 0.033f, 10, Mat::STEEL, 0.0f);        // frette sous la tête
    addBoard(m, vec3(0, TOP, 0), X, Y, Z, 0.058f, 0.06f, 0.046f, Mat::STEEL, 0.1f, r, 0.0f);                        // œil de la tête
    for (int sgn = -1; sgn <= 1; sgn += 2) {  // deux pointes courbes, faites de 9 tronçons de cône
        vec3 prev(sgn * 0.052f, TOP + 0.006f, 0);
        float rprev = 0.043f;
        const int N = 9;
        for (int k = 1; k <= N; k++) {
            float f = (float)k / N, x = sgn * (0.052f + 0.31f * f), y = TOP + 0.006f - 0.14f * f * f, rad = mixf(0.043f, 0.005f, std::pow(f, 0.75f));
            vec3 p(x, y, 0);
            addCylinder(m, prev, p, rprev, rad, 8, Mat::STEEL, 0.0f, false, k == N);
            prev = p;
            rprev = rad;
        }
    }
}

void buildLantern(Mesh& m) {
    Rng z(3);
    vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    addBoard(m, vec3(0, -0.11f, 0), X, Y, Z, 0.058f, 0.012f, 0.058f, Mat::STEEL, 0.1f, z, 0);  // socle
    for (int sx = -1; sx <= 1; sx += 2)  // 4 montants
        for (int sz = -1; sz <= 1; sz += 2) addBoard(m, vec3(sx * 0.052f, 0.0f, sz * 0.052f), Y, X, Z, 0.11f, 0.006f, 0.006f, Mat::STEEL, 0.1f, z, 0);
    addBoard(m, vec3(0, 0.118f, 0), X, Y, Z, 0.07f, 0.012f, 0.07f, Mat::STEEL, 0.1f, z, 0);  // chapeau
    addBoard(m, vec3(0, 0.15f, 0), X, Y, Z, 0.032f, 0.02f, 0.032f, Mat::STEEL, 0.1f, z, 0);
    addBoardBetween(m, vec3(-0.045f, 0.168f, 0), vec3(0, 0.24f, 0), Z, 0.008f, 0.008f, Mat::STEEL, z, 0, 0);  // anse
    addBoardBetween(m, vec3(0.045f, 0.168f, 0), vec3(0, 0.24f, 0), Z, 0.008f, 0.008f, Mat::STEEL, z, 0, 0);
    addEllipsoid(m, vec3(0), X, Y, Z, vec3(0.044f, 0.09f, 0.044f), Mat::LANTERN_GLASS, 0.0f);  // verre lumineux
}

void appendPickupMesh(Mesh& m, const Pickup& p, float T) {
    float a = T * 1.6f + p.phase;  // rotation sur lui-même
    vec3 ax(std::cos(a), 0, std::sin(a)), ay(0, 1, 0), az(-std::sin(a), 0, std::cos(a));
    vec3 c = p.p + vec3(0, 0.08f * std::sin(T * 2.0f + p.phase), 0);  // flottement
    Rng z(1);
    if (p.kind == 0) {  // clé : anneau, tige, deux dents
        addEllipsoid(m, c + ax * 0.13f, ax, ay, az, vec3(0.075f, 0.075f, 0.016f), Mat::GOLD, 0.0f);
        addBoard(m, c - ax * 0.06f, ax, ay, az, 0.17f, 0.018f, 0.018f, Mat::GOLD, 0.0f, z, 0);
        addBoard(m, c - ax * 0.19f - ay * 0.035f, ay, ax, az, 0.035f, 0.02f, 0.016f, Mat::GOLD, 0.0f, z, 0);
        addBoard(m, c - ax * 0.12f - ay * 0.03f, ay, ax, az, 0.03f, 0.02f, 0.016f, Mat::GOLD, 0.0f, z, 0);
    } else {  // fiole : panse rouge + bouchon de bois
        addEllipsoid(m, c, ax, ay, az, vec3(0.05f, 0.085f, 0.05f), Mat::POTION, 0.0f);
        addBoard(m, c + ay * 0.1f, ax, ay, az, 0.022f, 0.025f, 0.022f, Mat::WOOD, 0.2f, z, 0);
    }
}

void appendDynamiteMesh(Mesh& m, const Dynamite& d) {
    vec3 ax = d.R[0], ay = d.R[1], az = d.R[2];
    const vec2 off[3] = {vec2(0.0f, 0.024f), vec2(-0.021f, -0.012f), vec2(0.021f, -0.012f)};
    for (const vec2& o : off) {
        vec3 c = d.p + ay * o.y + az * o.x;
        addCylinder(m, c - ax * 0.1f, c + ax * 0.1f, 0.022f, 0.022f, 12, gCustomMats.dynamite, 0.0f);
    }
    for (float x : {-0.055f, 0.055f}) addCylinder(m, d.p + ax * (x - 0.012f), d.p + ax * (x + 0.012f), 0.047f, 0.047f, 14, Mat::STEEL, 0.0f, false, false);
    vec3 b = d.p + ax * 0.1f + ay * 0.024f, tip = fuseTip(d), mid = (b + tip) * 0.5f + ay * 0.012f;
    if (glm::length(tip - b) > 0.01f) {  // mèche en deux segments
        addCylinder(m, b, mid, 0.004f, 0.004f, 6, Mat::DARK_CLOTH, 0.0f);
        addCylinder(m, mid, tip, 0.004f, 0.004f, 6, Mat::DARK_CLOTH, 0.0f);
    }
}
