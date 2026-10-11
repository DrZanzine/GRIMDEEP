// =============================================================================
//  render/model_loader.cpp — lecteur OBJ/MTL minimal
// =============================================================================
#include "render/model_loader.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>

#include "core/paths.h"
#include "render/materials.h"

ModelLibrary gModels;

bool loadModel(const std::string& name, ModelData& md) {
    std::string base = assetPath("models/" + name);
    FILE* fm = std::fopen((base + ".mtl").c_str(), "r");
    FILE* fo = std::fopen((base + ".obj").c_str(), "r");
    if (!fm || !fo) {
        if (fm) std::fclose(fm);
        if (fo) std::fclose(fo);
        std::printf("Modele introuvable : %s (formes procedurales utilisees)\n", base.c_str());
        return false;
    }

    // ---- 1) matériaux (.mtl) : couleur diffuse Kd, émission Ke, spéculaire Ks / Ns
    struct MtlDef { vec3 kd = vec3(0.5f), ke = vec3(0), ks = vec3(0.05f); float ns = 10; };
    std::vector<std::pair<std::string, MtlDef>> mtls;
    char line[512];
    while (std::fgets(line, sizeof line, fm)) {
        char key[32], nm[128];
        float a, b, c;
        if (std::sscanf(line, "newmtl %127s", nm) == 1) mtls.push_back({nm, MtlDef()});
        else if (!mtls.empty() && std::sscanf(line, "%31s %f %f %f", key, &a, &b, &c) == 4) {
            MtlDef& d = mtls.back().second;
            if (!std::strcmp(key, "Kd")) d.kd = vec3(a, b, c);
            else if (!std::strcmp(key, "Ke")) d.ke = vec3(a, b, c);
            else if (!std::strcmp(key, "Ks")) d.ks = vec3(a, b, c);
        } else if (!mtls.empty() && std::sscanf(line, "Ns %f", &a) == 1) mtls.back().second.ns = a;
    }
    std::fclose(fm);

    // Noms de matériaux remplacés par les matériaux procéduraux du jeu.
    // (Les crocs de l'araignée utilisent l'ombrage « lave » : ils luisent dans le noir.)
    static const std::pair<const char*, float> GAME[] = {
        {"wood", Mat::WOOD},  {"steel", Mat::STEEL},     {"steel_dark", Mat::STEEL},  {"nail", Mat::STEEL},
        {"leather", Mat::DARK_CLOTH}, {"chitin", Mat::CHITIN}, {"chitin_dark", Mat::CHITIN}, {"fang", Mat::LAVA}};
    std::vector<float> matId;
    for (auto& m : mtls) {
        float id = -1;
        for (auto& g : GAME)
            if (m.first == g.first) id = g.second;
        if (id < 0) id = gMaterials.append(m.second.kd, m.second.ns, m.second.ke, m.second.ks.x);
        matId.push_back(id);
    }

    // ---- 2) géométrie (.obj) : positions, uv, normales, groupes, pivots, faces
    std::vector<vec3> P, N;
    std::vector<vec2> T;
    int cg = 0, cm = 0;  // groupe et matériau courants
    md.gname.assign(1, "body");
    md.gpivot.assign(1, vec3(0));
    std::vector<std::pair<std::string, vec3>> pivots;
    // Un sommet OBJ est un triplet (position, uv, normale) : on déduplique les triplets
    // identiques d'un même groupe et matériau pour partager les sommets.
    struct Key {
        int g, m, a, b, c;
        bool operator<(const Key& o) const { return std::tie(g, m, a, b, c) < std::tie(o.g, o.m, o.a, o.b, o.c); }
    };
    std::map<Key, uint32_t> cache;
    while (std::fgets(line, sizeof line, fo)) {
        char nm[128];
        float a, b, c;
        if (line[0] == 'v' && line[1] == ' ') { std::sscanf(line, "v %f %f %f", &a, &b, &c); P.push_back(vec3(a, b, c)); }
        else if (line[0] == 'v' && line[1] == 't') { std::sscanf(line, "vt %f %f", &a, &b); T.push_back(vec2(a, b)); }
        else if (line[0] == 'v' && line[1] == 'n') { std::sscanf(line, "vn %f %f %f", &a, &b, &c); N.push_back(vec3(a, b, c)); }
        else if (std::sscanf(line, "# pivot %127s %f %f %f", nm, &a, &b, &c) == 4) pivots.push_back({nm, vec3(a, b, c)});
        else if (line[0] == 'g' && std::sscanf(line, "g %127s", nm) == 1) {
            cg = md.group(nm);
            if (cg < 0) {
                md.gname.push_back(nm);
                md.gpivot.push_back(vec3(0));
                cg = (int)md.gname.size() - 1;
            }
        } else if (std::sscanf(line, "usemtl %127s", nm) == 1) {
            cm = 0;
            for (size_t i = 0; i < mtls.size(); i++)
                if (mtls[i].first == nm) cm = (int)i;
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::vector<uint32_t> ids;
            char* tok = std::strtok(line + 2, " \t\r\n");
            while (tok) {
                int vi = 0, ti = 0, ni = 0;
                std::sscanf(tok, "%d/%d/%d", &vi, &ti, &ni);
                Key k{cg, cm, vi, ti, ni};
                auto it = cache.find(k);
                if (it == cache.end()) {
                    Vertex vx;
                    vx.p = P[vi - 1]; vx.n = N[ni - 1]; vx.uv = T[ti - 1];
                    vx.mat = matId[cm]; vx.seed = 0; vx.biome = 0;
                    md.v.push_back(vx);
                    md.vg.push_back(cg);
                    it = cache.emplace(k, (uint32_t)md.v.size() - 1).first;
                }
                ids.push_back(it->second);
                tok = std::strtok(nullptr, " \t\r\n");
            }
            // polygone convexe -> éventail de triangles
            for (size_t k = 1; k + 1 < ids.size(); k++) {
                md.idx.push_back(ids[0]);
                md.idx.push_back(ids[k]);
                md.idx.push_back(ids[k + 1]);
            }
        }
    }
    std::fclose(fo);
    for (auto& pv : pivots) {
        int g = md.group(pv.first);
        if (g >= 0) md.gpivot[g] = pv.second;
    }
    md.ok = !md.idx.empty();
    std::printf("Modele %-18s %6zu sommets %6zu triangles %zu groupes\n", name.c_str(), md.v.size(), md.idx.size() / 3, md.gname.size());
    return md.ok;
}

void loadAllModels() {
    loadModel("planks_barricade", gModels.barricade);
    loadModel("ladder", gModels.ladder);
    loadModel("plank", gModels.plank);
    loadModel("mummy", gModels.mummy);
    loadModel("giant_spider", gModels.spider);
    loadModel("lava_golem", gModels.golem);
}

void appendModel(Mesh& m, const ModelData& md, const mat4& W, float seed, vec2 uvOff, const std::vector<mat4>* gx) {
    if (!md.ok) return;
    // Matrice finale de chaque groupe (et matrice des normales associée).
    size_t ng = md.gname.size();
    std::vector<mat4> M(ng, W);
    std::vector<mat3> NM(ng);
    for (size_t g = 0; g < ng; g++) {
        if (gx) M[g] = W * (*gx)[g];
        NM[g] = glm::transpose(glm::inverse(mat3(M[g])));
    }
    uint32_t base = (uint32_t)m.v.size();
    for (size_t i = 0; i < md.v.size(); i++) {
        const Vertex& s = md.v[i];
        int g = md.vg[i];
        Vertex o = s;
        o.p = vec3(M[g] * vec4(s.p, 1.0f));
        o.n = glm::normalize(NM[g] * s.n);
        o.uv = s.uv + uvOff;
        o.seed = seed;
        m.v.push_back(o);
    }
    for (uint32_t k : md.idx) m.idx.push_back(base + k);
}

void addPlankModel(Mesh& m, vec3 c, vec3 dl, vec3 dt, float hl, float hw, float ht, float seed) {
    if (!gModels.plank.ok) return;
    // Le modèle mesure 1,6 x 0,035 x 0,22 m : on l'étire pour qu'il épouse la boîte demandée.
    vec3 z = glm::normalize(glm::cross(dl, dt));
    mat4 R(1.0f);
    R[0] = vec4(dl * (hl / 0.8f), 0);
    R[1] = vec4(dt * (ht / 0.0175f), 0);
    R[2] = vec4(z * (hw / 0.11f), 0);
    R[3] = vec4(c, 1);
    appendModel(m, gModels.plank, R * glm::translate(mat4(1.0f), vec3(0, -0.0175f, 0)), seed, vec2(seed * 7.13f, seed * 3.71f));
}
