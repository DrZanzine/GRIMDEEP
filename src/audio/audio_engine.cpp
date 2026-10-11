// =============================================================================
//  audio/audio_engine.cpp — sortie WinMM, thread audio et mixage
// =============================================================================
#include "audio/audio_engine.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>

namespace aud {

/// Ressources Windows du moteur (cachées ici pour ne pas inclure windows.h partout).
struct Engine::Device {
    HWAVEOUT hwo = nullptr;     ///< sortie audio
    WAVEHDR hdr[NB];            ///< en-têtes des tampons
    short* buf[NB] = {};        ///< tampons (stéréo entrelacé)
    HANDLE thread = nullptr;    ///< thread audio
};

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::start() {
    dev.reset(new Device());
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = SR * 4;
    if (waveOutOpen(&dev->hwo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
    initSynthesis();
    for (int i = 0; i < NB; i++) {  // amorce : tous les tampons sont remplis et envoyés
        dev->buf[i] = new short[BS * 2]();
        dev->hdr[i] = WAVEHDR();
        dev->hdr[i].lpData = (LPSTR)dev->buf[i];
        dev->hdr[i].dwBufferLength = BS * 4;
        waveOutPrepareHeader(dev->hwo, &dev->hdr[i], sizeof(WAVEHDR));
        mix(dev->buf[i], BS);
        waveOutWrite(dev->hwo, &dev->hdr[i], sizeof(WAVEHDR));
    }
    run = true;
    dev->thread = CreateThread(nullptr, 0, threadMain, this, 0, nullptr);
    if (dev->thread) SetThreadPriority(dev->thread, THREAD_PRIORITY_TIME_CRITICAL);
    ok = true;
    return true;
}

void Engine::stop() {
    if (!ok) return;
    run = false;
    if (dev->thread) {
        WaitForSingleObject(dev->thread, 1000);
        CloseHandle(dev->thread);
    }
    waveOutReset(dev->hwo);
    for (int i = 0; i < NB; i++) {
        waveOutUnprepareHeader(dev->hwo, &dev->hdr[i], sizeof(WAVEHDR));
        delete[] dev->buf[i];
    }
    waveOutClose(dev->hwo);
    ok = false;
}

void Engine::sfx(int type, float vol, int var) {
    if (!ok || type == SF_DRIP) return;  // le son des gouttes a été retiré
    unsigned h = qhead.load();
    if (h - qtail.load() >= 60) return;  // file pleine : la demande est ignorée
    unsigned v = (unsigned)type | ((unsigned)(std::min(1.0f, std::max(0.0f, vol)) * 255) << 8) | ((unsigned)(var & 255) << 16);
    q[h % 64] = v;
    qhead = h + 1;
}

void Engine::initSynthesis() {
    for (int i = 0; i < NTRACKS; i++) tracks[i].init(i);
    rv[0].init(0);
    rv[1].init(1);
}

unsigned long __stdcall Engine::threadMain(void* p) {
    static_cast<Engine*>(p)->loop();
    return 0;
}

void Engine::loop() {
    while (run) {
        bool any = false;
        for (int i = 0; i < NB; i++)
            if (dev->hdr[i].dwFlags & WHDR_DONE) {  // tampon joué : on le recalcule et on le renvoie
                mix(dev->buf[i], BS);
                waveOutWrite(dev->hwo, &dev->hdr[i], sizeof(WAVEHDR));
                any = true;
            }
        if (!any) Sleep(2);
    }
}

void Engine::spawnSfx(unsigned v) {
    Sfx* s = nullptr;
    for (auto& x : sfxs)
        if (!x.on) { s = &x; break; }
    if (!s) s = &sfxs[0];  // plus de place : on remplace le premier
    *s = Sfx();
    s->on = true;
    s->type = v & 255;
    s->vol = ((v >> 8) & 255) / 255.0f;
    s->var = ((v >> 16) & 255) / 255.0f;
}

void Engine::mix(short* out, int frames) {
    const float dt = 1.0f / SR;
    int tg = target;
    float master = vMaster, music = vMusic, sfxv = vSfx, dk = duck;
    for (int i = 0; i < frames; i++) {
        // nouvelles demandes d'effets
        unsigned qt = qtail.load();
        while (qt != qhead.load()) { spawnSfx(q[qt % 64]); qt++; qtail = qt; }
        duckS += (dk - duckS) * 0.0004f;
        // musique : fondu enchaîné de 4 s vers le morceau demandé
        float L = 0, R = 0;
        for (int b = 0; b < NTRACKS; b++) {
            Track& t = tracks[b];
            float goal = (b == tg) ? 1.0f : 0.0f;
            if (t.gain < goal) t.gain = std::min(goal, t.gain + dt / 4.0f);
            else if (t.gain > goal) t.gain = std::max(goal, t.gain - dt / 4.0f);
            if (t.gain > 0.001f) {
                float l = 0, r = 0;
                t.render(l, r, dt);
                L += l * t.gain;
                R += r * t.gain;
            }
        }
        L *= music * duckS;
        R *= music * duckS;
        float wl = rv[0].process(L + R * 0.3f), wr = rv[1].process(R + L * 0.3f);  // réverbération
        L += wl * 0.38f;
        R += wr * 0.38f;
        // effets sonores (mono)
        float sl = 0;
        for (auto& s : sfxs)
            if (s.on) sl += sfxSample(s, rnd);
        sl *= sfxv;
        L += sl;
        R += sl;
        lpL += 0.62f * (L - lpL);  // adoucit les aigus (~5 kHz)
        lpR += 0.62f * (R - lpR);
        L = std::tanh(lpL * master * 1.15f);
        R = std::tanh(lpR * master * 1.15f);
        out[i * 2] = (short)(L * 28000);
        out[i * 2 + 1] = (short)(R * 28000);
    }
}

}  // namespace aud
