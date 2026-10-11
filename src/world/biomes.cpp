// =============================================================================
//  world/biomes.cpp — table des biomes
// =============================================================================
#include "world/biomes.h"

const BiomeDef BIOMES[BIOME_COUNT] = {
    //  nom                   teinte roche                 ambiance                       torches                      I     humid. mousse
    {"GALERIES BOISEES",   vec3(1.0f, 1.0f, 1.0f),    vec3(0.004f, 0.006f, 0.011f), vec3(1.00f, 0.50f, 0.17f), 4.2f, 0.50f, 0.0f},
    {"GROTTES HUMIDES",    vec3(0.72f, 0.95f, 0.92f), vec3(0.002f, 0.008f, 0.012f), vec3(0.30f, 0.78f, 1.00f), 3.4f, 0.90f, 1.0f},
    {"ANTRE DES CRISTAUX", vec3(0.90f, 0.82f, 1.05f), vec3(0.008f, 0.004f, 0.016f), vec3(0.72f, 0.32f, 1.00f), 2.6f, 0.30f, 0.0f},
    {"CATACOMBES",         vec3(1.12f, 1.08f, 0.92f), vec3(0.006f, 0.007f, 0.004f), vec3(0.78f, 0.95f, 0.35f), 2.5f, 0.20f, 0.0f},
    {"FORGE INFERNALE",    vec3(1.0f, 0.66f, 0.55f),  vec3(0.014f, 0.004f, 0.002f), vec3(1.00f, 0.28f, 0.07f), 2.9f, 0.10f, 0.0f},
    {"LA SORTIE",          vec3(0.95f, 0.97f, 1.0f),  vec3(0.020f, 0.025f, 0.030f), vec3(0.75f, 0.88f, 1.00f), 4.0f, 0.30f, 0.0f},
    {"LE CAMP DE BASE",    vec3(0.72f, 0.68f, 0.68f), vec3(0.15f, 0.13f, 0.25f),    vec3(1.00f, 0.55f, 0.20f), 4.2f, 0.15f, 0.55f}};
