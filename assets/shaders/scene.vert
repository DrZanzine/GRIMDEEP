#version 430 core
// scene.vert — sommets de toute la géométrie éclairée.
// Entrées : position, normale, uv, (matériau, graine), biome  (voir struct Vertex, src/render/mesh.h).
// uModel place l'objet (identité pour le décor, matrice propre pour la Dame et les objets en main).
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aN; layout(location=2) in vec2 aUV; layout(location=3) in vec2 aMS; layout(location=4) in float aBiome;
uniform mat4 uVP; uniform mat4 uModel;
out vec3 vPos; out vec3 vN; out vec2 vUV; flat out float vMat; out float vSeed; out float vBiome; out vec3 vLocal;
void main(){ vec4 w=uModel*vec4(aPos,1.0); vPos=w.xyz; vLocal=aPos; vN=mat3(uModel)*aN; vUV=aUV; vMat=aMS.x; vSeed=aMS.y; vBiome=aBiome; gl_Position=uVP*w; }
