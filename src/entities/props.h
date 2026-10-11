// =============================================================================
//  entities/props.h — objets construits en code : pioche, lanterne, clés, fioles, dynamite
// =============================================================================
#pragma once

#include "entities/dynamite.h"
#include "render/mesh.h"
#include "world/world.h"

/// Pioche vue en main (repère local) : manche en bois le long de +Y avec poignée de
/// cuir, tête d'acier courbe en travers (axe X) dont les pointes retombent vers le bas.
void buildPickaxe(Mesh& m);

/// Lanterne tenue dans la main gauche (repère local) : cage d'acier, verre lumineux, anse.
void buildLantern(Mesh& m);

/// Ajoute un objet à ramasser (clé dorée ou fiole rouge) qui tourne et flotte selon le temps `T`.
void appendPickupMesh(Mesh& m, const Pickup& p, float T);

/// Ajoute un bâton de dynamite : 3 bâtons rouges ligotés, deux bandes d'acier, mèche qui raccourcit.
void appendDynamiteMesh(Mesh& m, const Dynamite& d);
