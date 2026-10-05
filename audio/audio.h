// =============================================================================
//  audio.h — synthé procédural temps réel (WinMM waveOut)
//
//  6 morceaux composés, tous différents (tempo, tonalité, instruments) :
//    0 galeries boisées   : luth et flûte, ré dorien, folk de nains
//    1 grottes humides    : harpe et nappes, fa lydien, ambiant aquatique
//    2 antre des cristaux : boîte à musique à 3 temps, mi mineur, scintillant
//    3 catacombes         : bourdon, chœur en cluster, cloche funèbre, battement de cœur
//    4 forge infernale    : percussions, basse martelée, cuivres, ré phrygien
//    5 camp / menu / sortie : cordes et flûte héroïque, sol majeur
//  Aucune source de bruit blanc dans la musique (que des timbres harmoniques), réverbération de Schroeder,
//  réglages de volume (général / musique / effets) modifiables à chaud.
// =============================================================================
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace aud {

static const int SR = 44100;
static const int NTRACKS = 6;

struct Rnd {
    uint32_t s = 2463534242u;
    float f() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / 16777216.0f; }
    float r(float a, float b) { return a + (b - a) * f(); }
    int i(int n) { return (int)(f() * n) % n; }
};

static inline float midi(float m) { return 440.0f * std::pow(2.0f, (m - 69.0f) / 12.0f); }
static inline float sawc(double ph) { return (float)(2.0 * (ph - std::floor(ph + 0.5))); }
static inline float sinc(double ph) { return (float)std::sin(6.283185307179586 * ph); }

enum { V_PLUCK, V_BELL, V_PAD, V_STRINGS, V_FLUTE, V_CELESTA, V_CHOIR, V_THUMP, V_CLANG, V_BASS, V_BRASS, V_TICK };

struct Voice {
    bool on = false; int type = 0; double ph = 0, ph2 = 0, ph3 = 0;
    float f = 100, amp = 0.1f, pan = 0, age = 0, dur = 1, atk = 0.01f, rel = 0.3f, dec = 0, lp = 0, lpc = 0.1f;
};

struct Note { int step; float m; int len; };      // note de mélodie : pas de départ, hauteur midi, durée en pas

struct Track {
    int id = 0; Rnd rnd; std::vector<Voice> vs; double acc = 0; int step = 0; float gain = 0, time = 0;
    float stepSec = 0.2f;

    // ---------- création de voix ----------
    Voice* alloc() { for (auto& v : vs) if (!v.on) return &v; return &vs[0]; }
    Voice* voice_(int type, float m, float amp, float dur, float dec, float atk, float rel, float pan, float lpc) {
        Voice* v = alloc(); *v = Voice(); v->on = true; v->type = type; v->f = midi(m); v->amp = amp; v->dur = dur; v->dec = dec;
        v->atk = atk; v->rel = rel; v->pan = pan; v->lpc = lpc;
        return v;
    }
    void pad(int type, float m, float amp, float dur, float atk = 2.5f, float rel = 3.0f, float pan = 0, float lpc = 0.12f) { voice_(type, m, amp, dur, 0, atk, rel, pan, lpc); }
    void pluck(float m, float amp, float dec, float pan = 0) { voice_(V_PLUCK, m, amp, dec * 3.0f, dec, 0.003f, 0.3f, pan, 0); }
    void bell(float m, float amp, float dec, float pan = 0) { voice_(V_BELL, m, amp, dec * 3.5f, dec, 0.002f, 0.5f, pan, 0); }
    void celesta(float m, float amp, float dec, float pan = 0) { voice_(V_CELESTA, m, amp, dec * 3.0f, dec, 0.002f, 0.3f, pan, 0); }
    void flute(float m, float amp, float dur, float pan = 0) { voice_(V_FLUTE, m, amp, dur, 0, 0.06f, 0.18f, pan, 0); }
    void brass(float m, float amp, float dur, float pan = 0) { voice_(V_BRASS, m, amp, dur, 0, 0.05f, 0.12f, pan, 0.3f); }
    void bass(float m, float amp, float dur, float lpc = 0.1f) { voice_(V_BASS, m, amp, dur, 0, 0.004f, 0.08f, 0, lpc); }
    void thump(float hz, float amp, float dec) { voice_(V_THUMP, 69, amp, dec * 4, dec, 0.001f, 0.05f, 0, 0)->f = hz; }
    void clang(float m, float amp, float dec) { voice_(V_CLANG, m, amp, dec * 4, dec, 0.001f, 0.2f, 0, 0); }
    void tick(float m, float amp, float pan = 0) { voice_(V_TICK, m, amp, 0.05f, 0.012f, 0.0005f, 0.02f, pan, 0); }
    void melody(const std::vector<Note>& n, int pos, int type, float amp, float pan = 0) {
        for (const Note& x : n) if (x.step == pos) {
            float dur = x.len * stepSec;
            if (type == V_FLUTE) flute(x.m, amp, dur * 0.95f, pan); else if (type == V_BRASS) brass(x.m, amp, dur * 0.9f, pan); else pluck(x.m, amp, 0.8f, pan);
        }
    }

    void init(int t) {
        id = t; vs.assign(72, Voice()); rnd.s = 98765u + t * 7919u;
        static const float bpm[NTRACKS] = {76, 58, 88, 44, 104, 84};
        stepSec = (t == 2) ? 0.17f : 60.0f / bpm[t] / 4.0f;
        switch (t) {
            case 0: pad(V_STRINGS, 38, 0.022f, 1e9f, 4, 4, -0.2f, 0.05f); pad(V_STRINGS, 45, 0.016f, 1e9f, 5, 4, 0.2f, 0.05f); break;
            case 1: pad(V_PAD, 29, 0.05f, 1e9f, 4, 4); pad(V_PAD, 41, 0.025f, 1e9f, 6, 4, 0.2f); break;
            case 3: pad(V_CHOIR, 38, 0.05f, 1e9f, 6, 4, 0, 0); pad(V_CHOIR, 45, 0.03f, 1e9f, 7, 4, -0.4f, 0); pad(V_CHOIR, 51, 0.018f, 1e9f, 8, 4, 0.4f, 0); break;
            case 4: pad(V_STRINGS, 26, 0.04f, 1e9f, 2, 4, 0, 0.03f); break;
            default: break;
        }
    }

    // intervalles d'accords
    static const int* chordI(int q) {
        static const int a[4] = {0, 3, 7, 12}, b[4] = {0, 4, 7, 12}, c[4] = {0, 3, 7, 10}, d[4] = {0, 4, 7, 11};
        return q == 0 ? a : q == 1 ? b : q == 2 ? c : d;
    }

    void onStep() {
        int s = step++;
        switch (id) {
            case 0: {   // ---- galeries boisées : luth arpégé + flûte, ré dorien, 76 bpm
                static const int roots[8] = {50, 50, 46, 48, 50, 43, 48, 45}, qual[8] = {0, 0, 1, 1, 0, 0, 1, 0};
                static const std::vector<Note> A = {{0, 69, 6}, {6, 72, 2}, {8, 74, 6}, {14, 72, 2}, {16, 69, 8}, {24, 67, 4}, {28, 65, 4}};
                static const std::vector<Note> B = {{0, 65, 4}, {4, 67, 4}, {8, 69, 8}, {16, 72, 6}, {22, 69, 2}, {24, 67, 8}};
                static const std::vector<Note> C = {{0, 74, 8}, {8, 72, 4}, {12, 69, 4}, {16, 67, 6}, {22, 65, 2}, {24, 62, 8}};
                int bar = (s / 16) % 8, st = s % 16; const int* ch = chordI(qual[bar]); int r = roots[bar];
                static const int arp[8] = {0, 1, 2, 3, 2, 1, 2, 1};
                if (st % 2 == 0) pluck(r + 12 + ch[arp[st / 2]], st == 0 ? 0.12f : 0.085f, 0.8f, (st % 4 ? 0.35f : -0.35f));
                if (st == 0) pluck(r - 12, 0.15f, 1.4f);
                if (st == 8) pluck(r - 5, 0.10f, 1.2f);
                if (st == 0 || st == 8) thump(62, 0.09f, 0.12f);
                if (st == 4 || st == 12) tick(88, 0.014f, 0.3f);
                const std::vector<Note>& ph = ((s / 32) % 4 == 3) ? C : ((s / 32) % 2 == 1 ? B : A);
                melody(ph, s % 32, V_FLUTE, 0.085f, 0.1f);
            } break;
            case 1: {   // ---- grottes humides : nappes lydiennes + harpe, 58 bpm
                static const int ch[4][5] = {{53, 57, 60, 64, 69}, {55, 59, 62, 66, 71}, {50, 53, 57, 60, 64}, {57, 60, 64, 67, 71}};
                int bar = (s / 32) % 4, st = s % 32; const int* c = ch[bar];
                if (st == 0) for (int k = 0; k < 5; k++) pad(V_PAD, c[k], 0.03f, 33 * stepSec, 3.5f, 4.5f, (k - 2) * 0.28f);
                int m8 = s % 8;
                if (m8 == 0 || m8 == 3 || m8 == 5) {
                    int idx = (int)(((s / 8) * 3 + m8) % 5); float m = c[idx] + 12 * (1 + ((s / 16) % 2));
                    pluck(m, 0.06f, 1.8f, std::sin(s * 0.7f) * 0.7f);
                }
                if (st == 16 && rnd.f() < 0.8f) bell(c[3] + 24, 0.03f, 2.2f, rnd.r(-0.6f, 0.6f));
            } break;
            case 2: {   // ---- antre des cristaux : boîte à musique à 3 temps, mi mineur
                static const int ch[4][5] = {{52, 55, 59, 62, 66}, {48, 52, 55, 59, 64}, {55, 59, 62, 66, 71}, {50, 54, 57, 62, 66}};
                static const int pat[12] = {0, 2, 1, 3, 2, 4, 1, 3, 2, 4, 3, 2};
                int bar = (s / 12) % 4, st = s % 12; const int* c = ch[bar];
                if (st == 0) { for (int k = 0; k < 3; k++) pad(V_STRINGS, c[k] - 12, 0.02f, 12 * stepSec, 1.0f, 1.5f, (k - 1) * 0.5f, 0.07f); bass(c[0] - 12, 0.1f, 0.5f, 0.06f); }
                celesta(c[pat[st]] + 12 + 12 * (st % 4 == 3), st % 6 == 0 ? 0.075f : 0.05f, 0.5f, std::sin(s * 1.3f) * 0.8f);
                if (st == 6) bass(c[0] - 5, 0.06f, 0.35f, 0.06f);
                if (s % 48 == 0) bell(c[4] + 12, 0.05f, 2.0f, 0.3f);
                if (rnd.f() < 0.07f) celesta(c[rnd.i(5)] + 36, 0.03f, 0.9f, rnd.r(-0.9f, 0.9f));
            } break;
            case 3: {   // ---- catacombes : bourdon, chœur en cluster, glas, battement de cœur, 44 bpm
                static const int cl[3][3] = {{38, 45, 51}, {37, 44, 50}, {36, 43, 49}};
                int st = s % 16;
                if (s % 64 == 0 && s > 0) { const int* c = cl[(s / 64) % 3]; for (int k = 0; k < 3; k++) pad(V_CHOIR, c[k] + 12, 0.026f, 17.0f, 5.0f, 5.0f, (k - 1) * 0.5f, 0); }
                if (st == 0) thump(55, 0.12f, 0.14f);
                if (st == 3) thump(50, 0.07f, 0.12f);
                if (s % 32 == 8) bell(38, 0.07f, 3.5f, -0.1f);
                if (s % 48 == 20 && rnd.f() < 0.7f) pad(V_PAD, 86 + rnd.i(3) * 2, 0.012f, 4.0f, 2.0f, 3.0f, rnd.r(-0.8f, 0.8f));
            } break;
            case 4: {   // ---- forge infernale : percussions, basse martelée, cuivres, ré phrygien, 104 bpm
                static const int bassN[8] = {38, 38, 38, 39, 38, 38, 36, 39};
                static const int roots[4] = {38, 39, 36, 38};
                static const std::vector<Note> L = {{0, 74, 4}, {4, 75, 2}, {6, 74, 2}, {8, 72, 4}, {12, 70, 4}, {16, 69, 8}, {24, 70, 4}, {28, 72, 4},
                                                    {32, 74, 4}, {36, 77, 4}, {40, 75, 4}, {44, 74, 4}, {48, 72, 6}, {54, 70, 2}, {56, 69, 8}};
                int st = s % 16, bar = (s / 16) % 4;
                if (st == 0 || st == 6 || st == 8 || st == 14) thump(48, st % 8 == 0 ? 0.26f : 0.16f, 0.16f);
                if (st == 4 || st == 12) clang(62, 0.075f, 0.9f);
                if (st % 2 == 0) bass(bassN[(st / 2) % 8] + (bar == 1 ? 1 : bar == 2 ? -2 : 0), 0.13f, 0.17f, 0.09f);
                if (st % 2 == 1) tick(98, 0.012f, (st % 4 == 1) ? -0.4f : 0.4f);
                if (st == 0) { brass(roots[bar] + 12, 0.04f, 15 * stepSec, -0.2f); brass(roots[bar] + 19, 0.03f, 15 * stepSec, 0.2f); }
                if (s % 128 >= 64) melody(L, s % 64, V_BRASS, 0.06f, 0.0f);
            } break;
            case 5: {   // ---- camp / menu / sortie : cordes et flûte héroïque, sol majeur, 84 bpm
                static const int ch[4][4] = {{55, 59, 62, 67}, {50, 57, 62, 66}, {52, 55, 59, 64}, {48, 55, 60, 64}};
                static const std::vector<Note> M = {
                    {0, 74, 6}, {6, 71, 2}, {8, 67, 4}, {12, 69, 4}, {16, 71, 8}, {24, 74, 8}, {32, 76, 6}, {38, 74, 2}, {40, 72, 4}, {44, 71, 4}, {48, 69, 12}, {60, 71, 4},
                    {64, 79, 6}, {70, 76, 2}, {72, 74, 4}, {76, 71, 4}, {80, 72, 8}, {88, 71, 4}, {92, 69, 4}, {96, 71, 6}, {102, 74, 2}, {104, 76, 8}, {112, 71, 4}, {116, 67, 12}};
                int bar = (s / 16) % 4, st = s % 16; const int* c = ch[bar];
                if (st == 0) { for (int k = 0; k < 4; k++) pad(V_STRINGS, c[k], 0.032f, 16 * stepSec, 0.6f, 1.6f, (k - 1.5f) * 0.3f, 0.1f); pluck(c[0] - 12, 0.15f, 1.0f); }
                if (st == 8) pluck(c[0] - 5, 0.1f, 0.9f);
                if (s % 32 == 0) thump(58, 0.08f, 0.16f);
                if (st % 4 == 2 && rnd.f() < 0.35f) pluck(c[rnd.i(4)] + 24, 0.035f, 0.5f, rnd.r(-0.7f, 0.7f));
                melody(M, s % 128, V_FLUTE, 0.095f, 0.1f);
                if (s % 128 == 0) bell(79, 0.04f, 2.0f, 0.4f);
            } break;
        }
    }

    // ---------- synthèse d'une voix ----------
    float voice(Voice& v, float& panOut) {
        const float dt = 1.0f / SR; v.age += dt;
        float e = v.age < v.atk ? v.age / v.atk : 1.0f;
        if (v.dec > 0) e *= std::exp(-v.age / v.dec);
        if (v.age > v.dur) { e *= 1.0f - (v.age - v.dur) / v.rel; if (e <= 0) { v.on = false; return 0; } }
        if (v.dec > 0 && e < 2e-4f && v.age > v.atk) { v.on = false; return 0; }
        float s = 0; panOut = v.pan;
        switch (v.type) {
            case V_PLUCK: {
                v.ph += v.f * dt; float a2 = std::exp(-v.age / 0.35f), a3 = std::exp(-v.age / 0.15f);
                s = sinc(v.ph) + 0.45f * a2 * sinc(2 * v.ph) + 0.22f * a3 * sinc(3 * v.ph) + 0.1f * a3 * sinc(4 * v.ph);
                s *= 0.62f;
            } break;
            case V_BELL: {
                v.ph += v.f * dt; v.ph2 += v.f * 3.5 * dt; float e2 = std::exp(-v.age / 0.9f);
                s = (float)std::sin(6.2831853 * v.ph + 1.6f * e2 * std::sin(6.2831853 * v.ph2)) * 0.8f;
            } break;
            case V_PAD: {
                v.ph += v.f * dt; v.ph2 += v.f * 1.003 * dt; float lfo = 0.85f + 0.15f * std::sin(time * 0.31f + v.f * 0.01f);
                s = (sinc(v.ph) + sinc(v.ph2) + 0.25f * sinc(2 * v.ph)) * 0.45f * lfo;
            } break;
            case V_STRINGS: {
                v.ph += v.f * (1 + 0.0015 * std::sin(time * 4.8 + v.f)) * dt; v.ph2 += v.f * 1.006 * dt; v.ph3 += v.f * 0.994 * dt;
                float x = (sawc(v.ph) + sawc(v.ph2) + sawc(v.ph3)) * 0.33f; v.lp += v.lpc * (x - v.lp); s = v.lp * 2.0f;
            } break;
            case V_FLUTE: {
                float vib = 1.0f + 0.006f * std::sin(v.age * 5.4f) * std::min(1.0f, v.age * 3.0f);
                v.ph += v.f * vib * dt; s = (sinc(v.ph) + 0.22f * sinc(2 * v.ph) + 0.06f * sinc(3 * v.ph)) * 0.8f;
            } break;
            case V_CELESTA: {
                v.ph += v.f * dt; s = sinc(v.ph) + 0.35f * std::exp(-v.age / 0.15f) * sinc(4 * v.ph) + 0.18f * std::exp(-v.age / 0.07f) * sinc(6 * v.ph); s *= 0.7f;
            } break;
            case V_CHOIR: {
                float vib = 1.0f + 0.004f * std::sin(time * 4.6f + v.f); v.ph += v.f * vib * dt;
                s = (sinc(v.ph) + 0.5f * sinc(2 * v.ph) + 0.28f * sinc(3 * v.ph) + 0.12f * sinc(4 * v.ph)) * 0.55f;
            } break;
            case V_THUMP: { float fr = v.f * (0.55f + 1.45f * std::exp(-v.age / 0.05f)); v.ph += fr * dt; s = sinc(v.ph); } break;
            case V_CLANG: {
                v.ph += v.f * dt; s = 0.55f * sinc(v.ph) + 0.35f * sinc(v.ph * 2.76) + 0.2f * sinc(v.ph * 5.4) + 0.1f * sinc(v.ph * 8.93);
            } break;
            case V_BASS: {
                v.ph += v.f * dt; float x = sawc(v.ph) + 0.4f * sawc(v.ph * 2.0); v.lp += v.lpc * (x - v.lp); s = std::tanh(v.lp * 1.8f);
            } break;
            case V_BRASS: {
                v.ph += v.f * dt; float c = v.lpc * (0.35f + 0.65f * std::min(1.0f, v.age / 0.18f)); float x = sawc(v.ph); v.lp += c * (x - v.lp); s = v.lp * 1.6f;
            } break;
            case V_TICK: { v.ph += v.f * dt; s = sinc(v.ph); } break;
        }
        return s * e * v.amp;
    }

    void render(float& L, float& R, float dt) {
        static const float TG[NTRACKS] = {2.8f, 4.2f, 3.4f, 4.4f, 2.4f, 2.8f};      // équilibrage des niveaux entre morceaux
        const float k = TG[id];
        time += dt; acc += dt;
        while (acc >= stepSec) { acc -= stepSec; onStep(); }
        for (auto& v : vs) if (v.on) {
            float pan; float s = voice(v, pan);
            L += s * k * (0.5f - 0.5f * pan); R += s * k * (0.5f + 0.5f * pan);
        }
    }
};

// ----- effets sonores -------------------------------------------------------
enum { SF_DRIP, SF_SWING, SF_HITWOOD, SF_BREAK, SF_PICKUP, SF_DOOR, SF_HURT, SF_DIE, SF_ROAR, SF_CLICK, SF_HEART, SF_ROCK, SF_ROCKBREAK, SF_FLESH, SF_STEP, SF_GHOST, SF_COUNT };
struct Sfx { bool on = false; int type = 0; float t = 0, vol = 1, var = 0, lp = 0; double ph = 0, ph2 = 0; };

// réverbération de Schroeder (4 peignes + 2 passe-tout) par canal
struct Reverb {
    struct Comb { std::vector<float> b; size_t p = 0; float lp = 0; };
    Comb c[4]; std::vector<float> a[2]; size_t ap[2] = {0, 0};
    void init(int seed) {
        static const int cl[2][4] = {{1557, 1617, 1491, 1422}, {1580, 1640, 1514, 1445}}, al[2][2] = {{225, 556}, {248, 579}};
        for (int i = 0; i < 4; i++) c[i].b.assign((size_t)cl[seed][i] * SR / 44100, 0.0f);
        for (int i = 0; i < 2; i++) a[i].assign((size_t)al[seed][i] * SR / 44100, 0.0f);
    }
    float process(float x) {
        float o = 0;
        for (int i = 0; i < 4; i++) {
            Comb& k = c[i]; float y = k.b[k.p]; k.lp += 0.45f * (y - k.lp); k.b[k.p] = x + k.lp * 0.82f; k.p = (k.p + 1) % k.b.size(); o += y;
        }
        o *= 0.25f;
        for (int i = 0; i < 2; i++) { float d = a[i][ap[i]]; float y = -o + d; a[i][ap[i]] = o + d * 0.5f; ap[i] = (ap[i] + 1) % a[i].size(); o = y; }
        return o;
    }
};

struct Engine {
    HWAVEOUT hwo = nullptr; static const int NB = 6, BS = 1024;
    WAVEHDR hdr[NB]; short* buf[NB]; HANDLE thread = nullptr;
    std::atomic<bool> run{false}; std::atomic<int> target{0}; std::atomic<unsigned> qhead{0}, qtail{0}; std::atomic<unsigned> q[64];
    std::atomic<float> vMaster{0.8f}, vMusic{0.7f}, vSfx{0.8f}, duck{1.0f};
    Track tracks[NTRACKS]; Sfx sfxs[24]; Rnd rnd; Reverb rv[2];
    float lpL = 0, lpR = 0, duckS = 1.0f; bool ok = false;

    static DWORD WINAPI threadMain(LPVOID p) { ((Engine*)p)->loop(); return 0; }

    bool start() {
        WAVEFORMATEX fmt = {}; fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 2; fmt.nSamplesPerSec = SR; fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = SR * 4;
        if (waveOutOpen(&hwo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
        for (int i = 0; i < NTRACKS; i++) tracks[i].init(i);
        rv[0].init(0); rv[1].init(1);
        for (int i = 0; i < NB; i++) {
            buf[i] = new short[BS * 2](); hdr[i] = WAVEHDR(); hdr[i].lpData = (LPSTR)buf[i]; hdr[i].dwBufferLength = BS * 4;
            waveOutPrepareHeader(hwo, &hdr[i], sizeof(WAVEHDR)); fill(i); waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR));
        }
        run = true; thread = CreateThread(nullptr, 0, threadMain, this, 0, nullptr);
        if (thread) SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
        ok = true; return true;
    }
    void stop() {
        if (!ok) return; run = false; if (thread) { WaitForSingleObject(thread, 1000); CloseHandle(thread); }
        waveOutReset(hwo); for (int i = 0; i < NB; i++) { waveOutUnprepareHeader(hwo, &hdr[i], sizeof(WAVEHDR)); delete[] buf[i]; }
        waveOutClose(hwo); ok = false;
    }
    void setBiome(int b) { target = b < 0 ? 0 : (b >= NTRACKS ? NTRACKS - 1 : b); }
    void setVolumes(float master, float music, float sfxv) { vMaster = master; vMusic = music; vSfx = sfxv; }
    void sfx(int type, float vol = 1.0f, int var = 0) {
        if (!ok || type == SF_DRIP) return; unsigned h = qhead.load(); if (h - qtail.load() >= 60) return;
        unsigned v = (unsigned)type | ((unsigned)(std::min(1.0f, std::max(0.0f, vol)) * 255) << 8) | ((unsigned)(var & 255) << 16);
        q[h % 64] = v; qhead = h + 1;
    }
    void loop() {
        while (run) {
            bool any = false;
            for (int i = 0; i < NB; i++) if (hdr[i].dwFlags & WHDR_DONE) { fill(i); waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR)); any = true; }
            if (!any) Sleep(2);
        }
    }
    void spawnSfx(unsigned v) {
        Sfx* s = nullptr; for (auto& x : sfxs) if (!x.on) { s = &x; break; } if (!s) s = &sfxs[0];
        *s = Sfx(); s->on = true; s->type = v & 255; s->vol = ((v >> 8) & 255) / 255.0f; s->var = ((v >> 16) & 255) / 255.0f;
    }
    float sfxSample(Sfx& s) {
        const float dt = 1.0f / SR; s.t += dt; float t = s.t, o = 0;
        switch (s.type) {
            case SF_SWING: { float n = rnd.f() * 2 - 1; float env = std::sin(std::min(1.0f, t / 0.28f) * 3.1416f); s.lp += (0.02f + 0.18f * env) * (n - s.lp); o = s.lp * env * 1.1f; if (t > 0.3f) s.on = false; } break;
            case SF_HITWOOD: { s.ph += (60.0f + 90.0f * std::exp(-t / 0.03f)) * dt; float n = rnd.f() * 2 - 1; s.lp += 0.3f * (n - s.lp); o = sinc(s.ph) * std::exp(-t / 0.09f) * 0.8f + (n - s.lp) * std::exp(-t / 0.02f) * 0.35f; if (t > 0.5f) s.on = false; } break;
            case SF_BREAK: { float n = rnd.f() * 2 - 1; s.lp += 0.2f * (n - s.lp); s.ph += (50.0f + 60.0f * std::exp(-t / 0.05f)) * dt; o = (n - s.lp) * std::exp(-t / 0.18f) * 0.6f + sinc(s.ph) * std::exp(-t / 0.12f) * 0.7f; if (t > 0.9f) s.on = false; } break;
            case SF_PICKUP: { s.ph += (t < 0.09f ? 880.0f : 1318.5f) * dt; o = sinc(s.ph) * std::exp(-t / 0.25f) * 0.3f; if (t > 1.0f) s.on = false; } break;
            case SF_DOOR: { float n = rnd.f() * 2 - 1; s.lp += 0.012f * (n - s.lp); s.ph += (46.0f + 6.0f * std::sin(t * 3)) * dt; float env = std::min(1.0f, t / 0.2f) * std::max(0.0f, 1.0f - t / 2.4f); o = (s.lp * 7.0f + sinc(s.ph) * 0.6f) * env; if (t > 2.4f) s.on = false; } break;
            case SF_HURT: {   // coup reçu : choc sourd + râle descendant + petit tintement, bien reconnaissable
                s.ph += (95.0f - 45.0f * std::min(1.0f, t / 0.2f)) * dt; s.ph2 += (230.0f - 120.0f * std::min(1.0f, t / 0.35f)) * dt;
                float x = sawc(s.ph2); s.lp += 0.08f * (x - s.lp);
                o = sinc(s.ph) * std::exp(-t / 0.16f) * 0.9f + s.lp * std::exp(-t / 0.2f) * 0.55f; if (t > 0.7f) s.on = false;
            } break;
            case SF_DIE: { s.ph += (320.0f * std::exp(-t / 0.35f) + 50.0f) * dt; o = sinc(s.ph) * std::exp(-t / 0.4f) * 0.6f; if (t > 1.2f) s.on = false; } break;
            case SF_ROAR: { s.ph += (72.0f + 18.0f * std::sin(t * 9.0f)) * dt; float x = sawc(s.ph); s.lp += 0.05f * (x - s.lp); float env = std::min(1.0f, t / 0.1f) * std::max(0.0f, 1.0f - t / 0.6f); o = s.lp * 1.8f * env; if (t > 0.6f) s.on = false; } break;
            case SF_CLICK: { s.ph += (s.var > 0.5f ? 1320.0f : 880.0f) * dt; o = sinc(s.ph) * std::exp(-t / 0.035f) * 0.25f; if (t > 0.2f) s.on = false; } break;
            case SF_ROCK: {   // pioche sur la pierre : claquement sec + tintement métallique
                float n = rnd.f() * 2 - 1; s.lp += 0.5f * (n - s.lp); s.ph += (170.0f + 120.0f * std::exp(-t / 0.02f)) * dt; s.ph2 += (1450.0f + 300.0f * s.var) * dt;
                o = (n - s.lp) * std::exp(-t / 0.035f) * 0.9f + sinc(s.ph) * std::exp(-t / 0.08f) * 0.7f + sinc(s.ph2) * std::exp(-t / 0.05f) * 0.16f; if (t > 0.45f) s.on = false;
            } break;
            case SF_ROCKBREAK: {   // bloc qui éclate : craquement + grondement d'éboulis
                float n = rnd.f() * 2 - 1; s.lp += 0.12f * (n - s.lp); s.ph += (38.0f + 50.0f * std::exp(-t / 0.08f)) * dt;
                float roll = (n - s.lp) * (0.5f + 0.5f * std::sin(t * 70.0f)) * std::exp(-t / 0.5f);
                o = (n - s.lp) * std::exp(-t / 0.06f) * 0.9f + sinc(s.ph) * std::exp(-t / 0.3f) * 0.9f + roll * 0.55f; if (t > 1.3f) s.on = false;
            } break;
            case SF_FLESH: {   // coup qui porte : choc sourd humide
                float n = rnd.f() * 2 - 1; s.lp += 0.18f * (n - s.lp); s.ph += (82.0f - 40.0f * std::min(1.0f, t / 0.12f)) * dt;
                o = sinc(s.ph) * std::exp(-t / 0.1f) * 1.0f + s.lp * std::exp(-t / 0.05f) * 0.6f; if (t > 0.4f) s.on = false;
            } break;
            case SF_STEP: {   // pas : choc sourd + léger crissement (var = 1 sur l'herbe : plus mat)
                float n = rnd.f() * 2 - 1; s.lp += (s.var > 0.5f ? 0.07f : 0.2f) * (n - s.lp); s.ph += (62.0f - 20.0f * std::min(1.0f, t / 0.08f)) * dt;
                o = sinc(s.ph) * std::exp(-t / 0.05f) * 0.6f + s.lp * std::exp(-t / (s.var > 0.5f ? 0.06f : 0.035f)) * (s.var > 0.5f ? 0.7f : 0.5f); if (t > 0.25f) s.on = false;
            } break;
            case SF_GHOST: {   // apparition qui s'évanouit : souffle grave, gémissement qui retombe
                float n = rnd.f() * 2 - 1; s.lp += 0.03f * (n - s.lp); s.ph += (58.0f - 22.0f * std::min(1.0f, t / 1.4f)) * dt;
                s.ph2 += (340.0f + 35.0f * std::sin(t * 7.0f)) * (1.0f - 0.35f * std::min(1.0f, t / 1.6f)) * dt;
                float env = std::min(1.0f, t / 0.04f) * std::exp(-t / 0.75f);
                o = (sinc(s.ph) * 0.75f + s.lp * 6.0f + sinc(s.ph2) * 0.14f) * env; if (t > 2.0f) s.on = false;
            } break;
            case SF_HEART: { s.ph += 52.0f * (0.6f + 0.8f * std::exp(-t / 0.05f)) * dt; float e1 = std::exp(-t / 0.09f), e2 = t > 0.18f ? std::exp(-(t - 0.18f) / 0.09f) * 0.7f : 0.0f; o = sinc(s.ph) * (e1 + e2) * 0.8f; if (t > 0.6f) s.on = false; } break;
        }
        return o * s.vol;
    }
    void fill(int bi) {
        short* out = buf[bi]; const float dt = 1.0f / SR; int tg = target;
        float master = vMaster, music = vMusic, sfxv = vSfx, dk = duck;
        for (int i = 0; i < BS; i++) {
            unsigned qt = qtail.load(); while (qt != qhead.load()) { spawnSfx(q[qt % 64]); qt++; qtail = qt; }
            duckS += (dk - duckS) * 0.0004f;
            float L = 0, R = 0;
            for (int b = 0; b < NTRACKS; b++) {
                Track& t = tracks[b]; float goal = (b == tg) ? 1.0f : 0.0f;
                if (t.gain < goal) t.gain = std::min(goal, t.gain + dt / 4.0f); else if (t.gain > goal) t.gain = std::max(goal, t.gain - dt / 4.0f);
                if (t.gain > 0.001f) { float l = 0, r = 0; t.render(l, r, dt); L += l * t.gain; R += r * t.gain; }
            }
            L *= music * duckS; R *= music * duckS;
            float wl = rv[0].process(L + R * 0.3f), wr = rv[1].process(R + L * 0.3f);       // réverbération
            L += wl * 0.38f; R += wr * 0.38f;
            float sl = 0; for (auto& s : sfxs) if (s.on) sl += sfxSample(s);
            sl *= sfxv; L += sl; R += sl;
            lpL += 0.62f * (L - lpL); lpR += 0.62f * (R - lpR);                               // adoucit les aigus (~5 kHz)
            L = std::tanh(lpL * master * 1.15f); R = std::tanh(lpR * master * 1.15f);
            out[i * 2] = (short)(L * 28000); out[i * 2 + 1] = (short)(R * 28000);
        }
    }
};

} // namespace aud
