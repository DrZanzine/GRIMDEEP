// =============================================================================
//  audio/sound_effects.cpp — formules de synthèse des effets
//
//  Notations : t = temps depuis le début de l'effet, n = bruit blanc (-1..1),
//  s.lp = bruit filtré passe-bas, sinc / sawc = sinus / dent de scie (phase en tours).
// =============================================================================
#include "audio/sound_effects.h"

#include <algorithm>

namespace aud {

float sfxSample(Sfx& s, Rnd& rnd) {
    const float dt = 1.0f / SR;
    s.t += dt;
    float t = s.t, o = 0;
    switch (s.type) {
        case SF_SWING: {  // souffle filtré dont la brillance suit l'enveloppe
            float n = rnd.f() * 2 - 1;
            float env = std::sin(std::min(1.0f, t / 0.28f) * 3.1416f);
            s.lp += (0.02f + 0.18f * env) * (n - s.lp);
            o = s.lp * env * 1.1f;
            if (t > 0.3f) s.on = false;
        } break;
        case SF_HITWOOD: {
            s.ph += (60.0f + 90.0f * std::exp(-t / 0.03f)) * dt;
            float n = rnd.f() * 2 - 1;
            s.lp += 0.3f * (n - s.lp);
            o = sinc(s.ph) * std::exp(-t / 0.09f) * 0.8f + (n - s.lp) * std::exp(-t / 0.02f) * 0.35f;
            if (t > 0.5f) s.on = false;
        } break;
        case SF_BREAK: {
            float n = rnd.f() * 2 - 1;
            s.lp += 0.2f * (n - s.lp);
            s.ph += (50.0f + 60.0f * std::exp(-t / 0.05f)) * dt;
            o = (n - s.lp) * std::exp(-t / 0.18f) * 0.6f + sinc(s.ph) * std::exp(-t / 0.12f) * 0.7f;
            if (t > 0.9f) s.on = false;
        } break;
        case SF_PICKUP: {  // deux notes : la5 puis mi6
            s.ph += (t < 0.09f ? 880.0f : 1318.5f) * dt;
            o = sinc(s.ph) * std::exp(-t / 0.25f) * 0.3f;
            if (t > 1.0f) s.on = false;
        } break;
        case SF_DOOR: {
            float n = rnd.f() * 2 - 1;
            s.lp += 0.012f * (n - s.lp);
            s.ph += (46.0f + 6.0f * std::sin(t * 3)) * dt;
            float env = std::min(1.0f, t / 0.2f) * std::max(0.0f, 1.0f - t / 2.4f);
            o = (s.lp * 7.0f + sinc(s.ph) * 0.6f) * env;
            if (t > 2.4f) s.on = false;
        } break;
        case SF_HURT: {  // coup reçu : choc sourd + râle descendant + petit tintement, bien reconnaissable
            s.ph += (95.0f - 45.0f * std::min(1.0f, t / 0.2f)) * dt;
            s.ph2 += (230.0f - 120.0f * std::min(1.0f, t / 0.35f)) * dt;
            float x = sawc(s.ph2);
            s.lp += 0.08f * (x - s.lp);
            o = sinc(s.ph) * std::exp(-t / 0.16f) * 0.9f + s.lp * std::exp(-t / 0.2f) * 0.55f;
            if (t > 0.7f) s.on = false;
        } break;
        case SF_DIE: {
            s.ph += (320.0f * std::exp(-t / 0.35f) + 50.0f) * dt;
            o = sinc(s.ph) * std::exp(-t / 0.4f) * 0.6f;
            if (t > 1.2f) s.on = false;
        } break;
        case SF_ROAR: {
            s.ph += (72.0f + 18.0f * std::sin(t * 9.0f)) * dt;
            float x = sawc(s.ph);
            s.lp += 0.05f * (x - s.lp);
            float env = std::min(1.0f, t / 0.1f) * std::max(0.0f, 1.0f - t / 0.6f);
            o = s.lp * 1.8f * env;
            if (t > 0.6f) s.on = false;
        } break;
        case SF_CLICK: {
            s.ph += (s.var > 0.5f ? 1320.0f : 880.0f) * dt;
            o = sinc(s.ph) * std::exp(-t / 0.035f) * 0.25f;
            if (t > 0.2f) s.on = false;
        } break;
        case SF_ROCK: {  // pioche sur la pierre : claquement sec + tintement métallique
            float n = rnd.f() * 2 - 1;
            s.lp += 0.5f * (n - s.lp);
            s.ph += (170.0f + 120.0f * std::exp(-t / 0.02f)) * dt;
            s.ph2 += (1450.0f + 300.0f * s.var) * dt;
            o = (n - s.lp) * std::exp(-t / 0.035f) * 0.9f + sinc(s.ph) * std::exp(-t / 0.08f) * 0.7f + sinc(s.ph2) * std::exp(-t / 0.05f) * 0.16f;
            if (t > 0.45f) s.on = false;
        } break;
        case SF_ROCKBREAK: {  // bloc qui éclate : craquement + grondement d'éboulis
            float n = rnd.f() * 2 - 1;
            s.lp += 0.12f * (n - s.lp);
            s.ph += (38.0f + 50.0f * std::exp(-t / 0.08f)) * dt;
            float roll = (n - s.lp) * (0.5f + 0.5f * std::sin(t * 70.0f)) * std::exp(-t / 0.5f);
            o = (n - s.lp) * std::exp(-t / 0.06f) * 0.9f + sinc(s.ph) * std::exp(-t / 0.3f) * 0.9f + roll * 0.55f;
            if (t > 1.3f) s.on = false;
        } break;
        case SF_FLESH: {  // coup qui porte : choc sourd humide
            float n = rnd.f() * 2 - 1;
            s.lp += 0.18f * (n - s.lp);
            s.ph += (82.0f - 40.0f * std::min(1.0f, t / 0.12f)) * dt;
            o = sinc(s.ph) * std::exp(-t / 0.1f) * 1.0f + s.lp * std::exp(-t / 0.05f) * 0.6f;
            if (t > 0.4f) s.on = false;
        } break;
        case SF_STEP: {  // pas : choc sourd + léger crissement (var = 1 sur l'herbe : plus mat)
            float n = rnd.f() * 2 - 1;
            s.lp += (s.var > 0.5f ? 0.07f : 0.2f) * (n - s.lp);
            s.ph += (62.0f - 20.0f * std::min(1.0f, t / 0.08f)) * dt;
            o = sinc(s.ph) * std::exp(-t / 0.05f) * 0.6f + s.lp * std::exp(-t / (s.var > 0.5f ? 0.06f : 0.035f)) * (s.var > 0.5f ? 0.7f : 0.5f);
            if (t > 0.25f) s.on = false;
        } break;
        case SF_GHOST: {  // apparition qui s'évanouit : souffle grave, gémissement qui retombe
            float n = rnd.f() * 2 - 1;
            s.lp += 0.03f * (n - s.lp);
            s.ph += (58.0f - 22.0f * std::min(1.0f, t / 1.4f)) * dt;
            s.ph2 += (340.0f + 35.0f * std::sin(t * 7.0f)) * (1.0f - 0.35f * std::min(1.0f, t / 1.6f)) * dt;
            float env = std::min(1.0f, t / 0.04f) * std::exp(-t / 0.75f);
            o = (sinc(s.ph) * 0.75f + s.lp * 6.0f + sinc(s.ph2) * 0.14f) * env;
            if (t > 2.0f) s.on = false;
        } break;
        case SF_BOOM: {  // explosion : claquement sec, souffle, grondement grave qui roule, saturation douce
            float n = rnd.f() * 2 - 1;
            s.lp += 0.05f * (n - s.lp);
            s.ph += (52.0f * std::exp(-t / 0.22f) + 21.0f) * dt;
            s.ph2 += 31.0f * dt;
            float crack = n * std::exp(-t / 0.025f) * 1.6f, blast = (n * 0.5f + s.lp * 3.0f) * std::exp(-t / 0.18f);
            float rumble = s.lp * 6.0f * std::exp(-t / 1.3f) * (0.75f + 0.25f * sinc(s.ph2 * 0.13f)), sub = sinc(s.ph) * std::exp(-t / 0.9f) * 1.5f;
            o = crack + blast + rumble + sub;
            o = 1.5f * o / (1.0f + std::fabs(o));
            if (t > 4.0f) s.on = false;
        } break;
        case SF_RING: {  // deux sinus aigus proches (battement)
            s.ph += 3950.0f * dt;
            s.ph2 += 4010.0f * dt;
            o = (sinc(s.ph) + sinc(s.ph2)) * 0.03f * std::min(1.0f, t / 0.15f) * std::exp(-t / 1.4f);
            if (t > 5.0f) s.on = false;
        } break;
        case SF_FUSE: {
            float n = rnd.f() * 2 - 1;
            s.lp += 0.55f * (n - s.lp);
            o = (n - s.lp) * 0.22f * (0.55f + 0.45f * std::sin(t * 97.0f)) * std::min(1.0f, t / 0.02f);
            if (t > 0.26f) s.on = false;
        } break;
        case SF_HEART: {  // « boum-boum » : deux chocs graves
            s.ph += 52.0f * (0.6f + 0.8f * std::exp(-t / 0.05f)) * dt;
            float e1 = std::exp(-t / 0.09f), e2 = t > 0.18f ? std::exp(-(t - 0.18f) / 0.09f) * 0.7f : 0.0f;
            o = sinc(s.ph) * (e1 + e2) * 0.8f;
            if (t > 0.6f) s.on = false;
        } break;
    }
    return o * s.vol;
}

}  // namespace aud
