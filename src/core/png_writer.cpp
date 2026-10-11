// =============================================================================
//  core/png_writer.cpp — encodeur PNG minimal (deflate non compressé)
//
//  Structure d'un PNG : signature 8 octets, puis des « chunks » (longueur, type,
//  données, CRC32). On écrit IHDR (dimensions), IDAT (pixels compressés zlib) et IEND.
// =============================================================================
#include "core/png_writer.h"

#include <algorithm>
#include <cstdio>

void writePNG(const char* path, const std::vector<uint8_t>& rgb, int w, int h) {
    // --- table CRC32 (polynôme 0xEDB88320), calculée une seule fois
    static uint32_t tab[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
            tab[n] = c;
        }
        init = true;
    }
    auto crc = [&](const uint8_t* d, size_t n, uint32_t c = 0xFFFFFFFFU) {
        for (size_t i = 0; i < n; i++) c = tab[(c ^ d[i]) & 0xFF] ^ (c >> 8);
        return c;
    };

    // --- données brutes : chaque ligne est précédée de l'octet de filtre 0 (aucun filtre)
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
    }

    // --- flux zlib : en-tête, blocs deflate « stored » de 65535 octets max, somme Adler-32
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t o = 0; o < raw.size(); o += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - o);
        bool last = o + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 255);
        z.push_back(n >> 8);
        z.push_back(~n & 255);
        z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + o, raw.begin() + o + n);
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    uint32_t ad = (b << 16) | a;
    for (int i = 3; i >= 0; i--) z.push_back((ad >> (8 * i)) & 255);

    // --- écriture du fichier
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    auto be32 = [&](uint32_t v) {  // entier 32 bits gros-boutiste
        uint8_t b4[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
        std::fwrite(b4, 1, 4, f);
    };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& d) {
        be32((uint32_t)d.size());
        std::fwrite(type, 1, 4, f);
        if (!d.empty()) std::fwrite(d.data(), 1, d.size(), f);
        std::vector<uint8_t> t(type, type + 4);  // le CRC couvre le type + les données
        t.insert(t.end(), d.begin(), d.end());
        be32(~crc(t.data(), t.size()));
    };
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr(13, 0);
    for (int i = 0; i < 4; i++) {
        ihdr[i] = (w >> (24 - 8 * i)) & 255;
        ihdr[4 + i] = (h >> (24 - 8 * i)) & 255;
    }
    ihdr[8] = 8;  // 8 bits par canal
    ihdr[9] = 2;  // couleur RVB
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    std::fclose(f);
}
