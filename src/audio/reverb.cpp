// =============================================================================
//  audio/reverb.cpp — implémentation de la réverbération
// =============================================================================
#include "audio/reverb.h"

#include "audio/synth.h"

namespace aud {

void Reverb::init(int seed) {
    // longueurs (en échantillons à 44,1 kHz) premières entre elles : pas d'écho régulier
    static const int cl[2][4] = {{1557, 1617, 1491, 1422}, {1580, 1640, 1514, 1445}}, al[2][2] = {{225, 556}, {248, 579}};
    for (int i = 0; i < 4; i++) c[i].b.assign((size_t)cl[seed][i] * SR / 44100, 0.0f);
    for (int i = 0; i < 2; i++) a[i].assign((size_t)al[seed][i] * SR / 44100, 0.0f);
}

float Reverb::process(float x) {
    float o = 0;
    for (int i = 0; i < 4; i++) {  // peignes en parallèle (réinjection 0,82, amortissement des aigus)
        Comb& k = c[i];
        float y = k.b[k.p];
        k.lp += 0.45f * (y - k.lp);
        k.b[k.p] = x + k.lp * 0.82f;
        k.p = (k.p + 1) % k.b.size();
        o += y;
    }
    o *= 0.25f;
    for (int i = 0; i < 2; i++) {  // passe-tout en série (diffusion)
        float d = a[i][ap[i]];
        float y = -o + d;
        a[i][ap[i]] = o + d * 0.5f;
        ap[i] = (ap[i] + 1) % a[i].size();
        o = y;
    }
    return o;
}

}  // namespace aud
