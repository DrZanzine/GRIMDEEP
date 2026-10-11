// =============================================================================
//  audio/music.cpp — partitions et synthèse des instruments
// =============================================================================
#include "audio/music.h"

#include <algorithm>

namespace aud {

// ---------------------------------------------------------------------------
//  Création de voix
// ---------------------------------------------------------------------------

Voice* Track::alloc() {
    for (auto& v : vs)
        if (!v.on) return &v;
    return &vs[0];
}

Voice* Track::voice_(int type, float m, float amp, float dur, float dec, float atk, float rel, float pan, float lpc) {
    Voice* v = alloc();
    *v = Voice();
    v->on = true; v->type = type; v->f = midi(m); v->amp = amp; v->dur = dur; v->dec = dec;
    v->atk = atk; v->rel = rel; v->pan = pan; v->lpc = lpc;
    return v;
}

void Track::pad(int type, float m, float amp, float dur, float atk, float rel, float pan, float lpc) { voice_(type, m, amp, dur, 0, atk, rel, pan, lpc); }
void Track::pluck(float m, float amp, float dec, float pan) { voice_(V_PLUCK, m, amp, dec * 3.0f, dec, 0.003f, 0.3f, pan, 0); }
void Track::bell(float m, float amp, float dec, float pan) { voice_(V_BELL, m, amp, dec * 3.5f, dec, 0.002f, 0.5f, pan, 0); }
void Track::celesta(float m, float amp, float dec, float pan) { voice_(V_CELESTA, m, amp, dec * 3.0f, dec, 0.002f, 0.3f, pan, 0); }
void Track::flute(float m, float amp, float dur, float pan) { voice_(V_FLUTE, m, amp, dur, 0, 0.06f, 0.18f, pan, 0); }
void Track::brass(float m, float amp, float dur, float pan) { voice_(V_BRASS, m, amp, dur, 0, 0.05f, 0.12f, pan, 0.3f); }
void Track::bass(float m, float amp, float dur, float lpc) { voice_(V_BASS, m, amp, dur, 0, 0.004f, 0.08f, 0, lpc); }
void Track::thump(float hz, float amp, float dec) { voice_(V_THUMP, 69, amp, dec * 4, dec, 0.001f, 0.05f, 0, 0)->f = hz; }
void Track::clang(float m, float amp, float dec) { voice_(V_CLANG, m, amp, dec * 4, dec, 0.001f, 0.2f, 0, 0); }
void Track::tick(float m, float amp, float pan) { voice_(V_TICK, m, amp, 0.05f, 0.012f, 0.0005f, 0.02f, pan, 0); }

void Track::melody(const std::vector<Note>& n, int pos, int type, float amp, float pan) {
    for (const Note& x : n)
        if (x.step == pos) {
            float dur = x.len * stepSec;
            if (type == V_FLUTE) flute(x.m, amp, dur * 0.95f, pan);
            else if (type == V_BRASS) brass(x.m, amp, dur * 0.9f, pan);
            else pluck(x.m, amp, 0.8f, pan);
        }
}

// ---------------------------------------------------------------------------
//  Partitions
// ---------------------------------------------------------------------------

void Track::init(int t) {
    id = t;
    vs.assign(72, Voice());
    rnd.s = 98765u + t * 7919u;
    static const float bpm[NTRACKS] = {76, 58, 88, 44, 104, 84};
    stepSec = (t == 2) ? 0.17f : 60.0f / bpm[t] / 4.0f;  // un pas = une double-croche
    // nappes de fond permanentes (durée « infinie »)
    switch (t) {
        case 0: pad(V_STRINGS, 38, 0.022f, 1e9f, 4, 4, -0.2f, 0.05f); pad(V_STRINGS, 45, 0.016f, 1e9f, 5, 4, 0.2f, 0.05f); break;
        case 1: pad(V_PAD, 29, 0.05f, 1e9f, 4, 4); pad(V_PAD, 41, 0.025f, 1e9f, 6, 4, 0.2f); break;
        case 3: pad(V_CHOIR, 38, 0.05f, 1e9f, 6, 4, 0, 0); pad(V_CHOIR, 45, 0.03f, 1e9f, 7, 4, -0.4f, 0); pad(V_CHOIR, 51, 0.018f, 1e9f, 8, 4, 0.4f, 0); break;
        case 4: pad(V_STRINGS, 26, 0.04f, 1e9f, 2, 4, 0, 0.03f); break;
        default: break;
    }
}

const int* Track::chordI(int q) {
    static const int a[4] = {0, 3, 7, 12}, b[4] = {0, 4, 7, 12}, c[4] = {0, 3, 7, 10}, d[4] = {0, 4, 7, 11};
    return q == 0 ? a : q == 1 ? b : q == 2 ? c : d;
}

void Track::onStep() {
    int s = step++;
    switch (id) {
        case 0: {  // ---- galeries boisées : luth arpégé + flûte, ré dorien, 76 bpm
            static const int roots[8] = {50, 50, 46, 48, 50, 43, 48, 45}, qual[8] = {0, 0, 1, 1, 0, 0, 1, 0};
            static const std::vector<Note> A = {{0, 69, 6}, {6, 72, 2}, {8, 74, 6}, {14, 72, 2}, {16, 69, 8}, {24, 67, 4}, {28, 65, 4}};
            static const std::vector<Note> B = {{0, 65, 4}, {4, 67, 4}, {8, 69, 8}, {16, 72, 6}, {22, 69, 2}, {24, 67, 8}};
            static const std::vector<Note> C = {{0, 74, 8}, {8, 72, 4}, {12, 69, 4}, {16, 67, 6}, {22, 65, 2}, {24, 62, 8}};
            int bar = (s / 16) % 8, st = s % 16;
            const int* ch = chordI(qual[bar]);
            int r = roots[bar];
            static const int arp[8] = {0, 1, 2, 3, 2, 1, 2, 1};
            if (st % 2 == 0) pluck(r + 12 + ch[arp[st / 2]], st == 0 ? 0.12f : 0.085f, 0.8f, (st % 4 ? 0.35f : -0.35f));
            if (st == 0) pluck(r - 12, 0.15f, 1.4f);
            if (st == 8) pluck(r - 5, 0.10f, 1.2f);
            if (st == 0 || st == 8) thump(62, 0.09f, 0.12f);
            if (st == 4 || st == 12) tick(88, 0.014f, 0.3f);
            const std::vector<Note>& ph = ((s / 32) % 4 == 3) ? C : ((s / 32) % 2 == 1 ? B : A);
            melody(ph, s % 32, V_FLUTE, 0.085f, 0.1f);
        } break;
        case 1: {  // ---- grottes humides : nappes lydiennes + harpe, 58 bpm
            static const int ch[4][5] = {{53, 57, 60, 64, 69}, {55, 59, 62, 66, 71}, {50, 53, 57, 60, 64}, {57, 60, 64, 67, 71}};
            int bar = (s / 32) % 4, st = s % 32;
            const int* c = ch[bar];
            if (st == 0)
                for (int k = 0; k < 5; k++) pad(V_PAD, c[k], 0.03f, 33 * stepSec, 3.5f, 4.5f, (k - 2) * 0.28f);
            int m8 = s % 8;
            if (m8 == 0 || m8 == 3 || m8 == 5) {
                int idx = (int)(((s / 8) * 3 + m8) % 5);
                float m = c[idx] + 12 * (1 + ((s / 16) % 2));
                pluck(m, 0.06f, 1.8f, std::sin(s * 0.7f) * 0.7f);
            }
            if (st == 16 && rnd.f() < 0.8f) bell(c[3] + 24, 0.03f, 2.2f, rnd.r(-0.6f, 0.6f));
        } break;
        case 2: {  // ---- antre des cristaux : boîte à musique à 3 temps, mi mineur
            static const int ch[4][5] = {{52, 55, 59, 62, 66}, {48, 52, 55, 59, 64}, {55, 59, 62, 66, 71}, {50, 54, 57, 62, 66}};
            static const int pat[12] = {0, 2, 1, 3, 2, 4, 1, 3, 2, 4, 3, 2};
            int bar = (s / 12) % 4, st = s % 12;
            const int* c = ch[bar];
            if (st == 0) {
                for (int k = 0; k < 3; k++) pad(V_STRINGS, c[k] - 12, 0.02f, 12 * stepSec, 1.0f, 1.5f, (k - 1) * 0.5f, 0.07f);
                bass(c[0] - 12, 0.1f, 0.5f, 0.06f);
            }
            celesta(c[pat[st]] + 12 + 12 * (st % 4 == 3), st % 6 == 0 ? 0.075f : 0.05f, 0.5f, std::sin(s * 1.3f) * 0.8f);
            if (st == 6) bass(c[0] - 5, 0.06f, 0.35f, 0.06f);
            if (s % 48 == 0) bell(c[4] + 12, 0.05f, 2.0f, 0.3f);
            if (rnd.f() < 0.07f) celesta(c[rnd.i(5)] + 36, 0.03f, 0.9f, rnd.r(-0.9f, 0.9f));
        } break;
        case 3: {  // ---- catacombes : bourdon, chœur en cluster, glas, battement de cœur, 44 bpm
            static const int cl[3][3] = {{38, 45, 51}, {37, 44, 50}, {36, 43, 49}};
            int st = s % 16;
            if (s % 64 == 0 && s > 0) {
                const int* c = cl[(s / 64) % 3];
                for (int k = 0; k < 3; k++) pad(V_CHOIR, c[k] + 12, 0.026f, 17.0f, 5.0f, 5.0f, (k - 1) * 0.5f, 0);
            }
            if (st == 0) thump(55, 0.12f, 0.14f);
            if (st == 3) thump(50, 0.07f, 0.12f);
            if (s % 32 == 8) bell(38, 0.07f, 3.5f, -0.1f);
            if (s % 48 == 20 && rnd.f() < 0.7f) pad(V_PAD, 86 + rnd.i(3) * 2, 0.012f, 4.0f, 2.0f, 3.0f, rnd.r(-0.8f, 0.8f));
        } break;
        case 4: {  // ---- forge infernale : percussions, basse martelée, cuivres, ré phrygien, 104 bpm
            static const int bassN[8] = {38, 38, 38, 39, 38, 38, 36, 39};
            static const int roots[4] = {38, 39, 36, 38};
            static const std::vector<Note> L = {{0, 74, 4}, {4, 75, 2}, {6, 74, 2}, {8, 72, 4}, {12, 70, 4}, {16, 69, 8}, {24, 70, 4}, {28, 72, 4},
                                                {32, 74, 4}, {36, 77, 4}, {40, 75, 4}, {44, 74, 4}, {48, 72, 6}, {54, 70, 2}, {56, 69, 8}};
            int st = s % 16, bar = (s / 16) % 4;
            if (st == 0 || st == 6 || st == 8 || st == 14) thump(48, st % 8 == 0 ? 0.26f : 0.16f, 0.16f);
            if (st == 4 || st == 12) clang(62, 0.075f, 0.9f);
            if (st % 2 == 0) bass(bassN[(st / 2) % 8] + (bar == 1 ? 1 : bar == 2 ? -2 : 0), 0.13f, 0.17f, 0.09f);
            if (st % 2 == 1) tick(98, 0.012f, (st % 4 == 1) ? -0.4f : 0.4f);
            if (st == 0) {
                brass(roots[bar] + 12, 0.04f, 15 * stepSec, -0.2f);
                brass(roots[bar] + 19, 0.03f, 15 * stepSec, 0.2f);
            }
            if (s % 128 >= 64) melody(L, s % 64, V_BRASS, 0.06f, 0.0f);
        } break;
        case 5: {  // ---- camp / menu / sortie : cordes et flûte héroïque, sol majeur, 84 bpm
            static const int ch[4][4] = {{55, 59, 62, 67}, {50, 57, 62, 66}, {52, 55, 59, 64}, {48, 55, 60, 64}};
            static const std::vector<Note> M = {
                {0, 74, 6},  {6, 71, 2},  {8, 67, 4},  {12, 69, 4}, {16, 71, 8}, {24, 74, 8},  {32, 76, 6},   {38, 74, 2},
                {40, 72, 4}, {44, 71, 4}, {48, 69, 12}, {60, 71, 4}, {64, 79, 6}, {70, 76, 2},  {72, 74, 4},   {76, 71, 4},
                {80, 72, 8}, {88, 71, 4}, {92, 69, 4}, {96, 71, 6}, {102, 74, 2}, {104, 76, 8}, {112, 71, 4}, {116, 67, 12}};
            int bar = (s / 16) % 4, st = s % 16;
            const int* c = ch[bar];
            if (st == 0) {
                for (int k = 0; k < 4; k++) pad(V_STRINGS, c[k], 0.032f, 16 * stepSec, 0.6f, 1.6f, (k - 1.5f) * 0.3f, 0.1f);
                pluck(c[0] - 12, 0.15f, 1.0f);
            }
            if (st == 8) pluck(c[0] - 5, 0.1f, 0.9f);
            if (s % 32 == 0) thump(58, 0.08f, 0.16f);
            if (st % 4 == 2 && rnd.f() < 0.35f) pluck(c[rnd.i(4)] + 24, 0.035f, 0.5f, rnd.r(-0.7f, 0.7f));
            melody(M, s % 128, V_FLUTE, 0.095f, 0.1f);
            if (s % 128 == 0) bell(79, 0.04f, 2.0f, 0.4f);
        } break;
    }
}

// ---------------------------------------------------------------------------
//  Synthèse
// ---------------------------------------------------------------------------

float Track::voice(Voice& v, float& panOut) {
    const float dt = 1.0f / SR;
    v.age += dt;
    // enveloppe : attaque linéaire, décroissance exponentielle optionnelle, relâchement linéaire
    float e = v.age < v.atk ? v.age / v.atk : 1.0f;
    if (v.dec > 0) e *= std::exp(-v.age / v.dec);
    if (v.age > v.dur) {
        e *= 1.0f - (v.age - v.dur) / v.rel;
        if (e <= 0) { v.on = false; return 0; }
    }
    if (v.dec > 0 && e < 2e-4f && v.age > v.atk) { v.on = false; return 0; }  // devenue inaudible
    float s = 0;
    panOut = v.pan;
    switch (v.type) {
        case V_PLUCK: {  // harmoniques qui s'éteignent plus vite que la fondamentale
            v.ph += v.f * dt;
            float a2 = std::exp(-v.age / 0.35f), a3 = std::exp(-v.age / 0.15f);
            s = sinc(v.ph) + 0.45f * a2 * sinc(2 * v.ph) + 0.22f * a3 * sinc(3 * v.ph) + 0.1f * a3 * sinc(4 * v.ph);
            s *= 0.62f;
        } break;
        case V_BELL: {  // modulation de fréquence (rapport 3,5 : timbre inharmonique de cloche)
            v.ph += v.f * dt;
            v.ph2 += v.f * 3.5 * dt;
            float e2 = std::exp(-v.age / 0.9f);
            s = (float)std::sin(6.2831853 * v.ph + 1.6f * e2 * std::sin(6.2831853 * v.ph2)) * 0.8f;
        } break;
        case V_PAD: {  // deux sinus légèrement désaccordés + LFO lent
            v.ph += v.f * dt;
            v.ph2 += v.f * 1.003 * dt;
            float lfo = 0.85f + 0.15f * std::sin(time * 0.31f + v.f * 0.01f);
            s = (sinc(v.ph) + sinc(v.ph2) + 0.25f * sinc(2 * v.ph)) * 0.45f * lfo;
        } break;
        case V_STRINGS: {  // trois dents de scie désaccordées, filtrées
            v.ph += v.f * (1 + 0.0015 * std::sin(time * 4.8 + v.f)) * dt;
            v.ph2 += v.f * 1.006 * dt;
            v.ph3 += v.f * 0.994 * dt;
            float x = (sawc(v.ph) + sawc(v.ph2) + sawc(v.ph3)) * 0.33f;
            v.lp += v.lpc * (x - v.lp);
            s = v.lp * 2.0f;
        } break;
        case V_FLUTE: {  // sinus + harmoniques faibles, vibrato qui s'installe
            float vib = 1.0f + 0.006f * std::sin(v.age * 5.4f) * std::min(1.0f, v.age * 3.0f);
            v.ph += v.f * vib * dt;
            s = (sinc(v.ph) + 0.22f * sinc(2 * v.ph) + 0.06f * sinc(3 * v.ph)) * 0.8f;
        } break;
        case V_CELESTA: {
            v.ph += v.f * dt;
            s = sinc(v.ph) + 0.35f * std::exp(-v.age / 0.15f) * sinc(4 * v.ph) + 0.18f * std::exp(-v.age / 0.07f) * sinc(6 * v.ph);
            s *= 0.7f;
        } break;
        case V_CHOIR: {
            float vib = 1.0f + 0.004f * std::sin(time * 4.6f + v.f);
            v.ph += v.f * vib * dt;
            s = (sinc(v.ph) + 0.5f * sinc(2 * v.ph) + 0.28f * sinc(3 * v.ph) + 0.12f * sinc(4 * v.ph)) * 0.55f;
        } break;
        case V_THUMP: {  // sinus dont la fréquence chute rapidement (grosse caisse)
            float fr = v.f * (0.55f + 1.45f * std::exp(-v.age / 0.05f));
            v.ph += fr * dt;
            s = sinc(v.ph);
        } break;
        case V_CLANG: {  // partiels inharmoniques (métal)
            v.ph += v.f * dt;
            s = 0.55f * sinc(v.ph) + 0.35f * sinc(v.ph * 2.76) + 0.2f * sinc(v.ph * 5.4) + 0.1f * sinc(v.ph * 8.93);
        } break;
        case V_BASS: {
            v.ph += v.f * dt;
            float x = sawc(v.ph) + 0.4f * sawc(v.ph * 2.0);
            v.lp += v.lpc * (x - v.lp);
            s = std::tanh(v.lp * 1.8f);
        } break;
        case V_BRASS: {  // dent de scie dont le filtre s'ouvre pendant l'attaque
            v.ph += v.f * dt;
            float c = v.lpc * (0.35f + 0.65f * std::min(1.0f, v.age / 0.18f));
            float x = sawc(v.ph);
            v.lp += c * (x - v.lp);
            s = v.lp * 1.6f;
        } break;
        case V_TICK: {
            v.ph += v.f * dt;
            s = sinc(v.ph);
        } break;
    }
    return s * e * v.amp;
}

void Track::render(float& L, float& R, float dt) {
    static const float TG[NTRACKS] = {2.8f, 4.2f, 3.4f, 4.4f, 2.4f, 2.8f};  // équilibrage des niveaux entre morceaux
    const float k = TG[id];
    time += dt;
    acc += dt;
    while (acc >= stepSec) { acc -= stepSec; onStep(); }
    for (auto& v : vs)
        if (v.on) {
            float pan;
            float s = voice(v, pan);
            L += s * k * (0.5f - 0.5f * pan);
            R += s * k * (0.5f + 0.5f * pan);
        }
}

}  // namespace aud
