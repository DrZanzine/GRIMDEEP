#version 430 core
// bloom_down.frag — réduction de moitié (filtre 13 échantillons). Au premier niveau (uFirst),
// ne garde que ce qui dépasse une luminosité de 0.7 (seuil du halo lumineux).
in vec2 uv; out vec4 fc; uniform sampler2D uTex; uniform vec2 uTexel; uniform int uFirst;
vec3 T(float x,float y){ return texture(uTex,uv+vec2(x,y)*uTexel).rgb; }
void main(){
  vec3 A=T(-2,-2),B=T(0,-2),C=T(2,-2),D=T(-1,-1),E=T(1,-1),F=T(-2,0),G=T(0,0),H=T(2,0),I=T(-1,1),J=T(1,1),K=T(-2,2),L=T(0,2),M=T(2,2);
  vec3 c=(D+E+I+J)*0.125+(A+B+F+G)*0.03125+(B+C+G+H)*0.03125+(F+G+K+L)*0.03125+(G+H+L+M)*0.03125;
  if(uFirst==1){ float br=max(c.r,max(c.g,c.b)); c*=max(br-0.7,0.0)/max(br,1e-4); c=min(c,vec3(30.0)); }
  fc=vec4(c,1.0); }
