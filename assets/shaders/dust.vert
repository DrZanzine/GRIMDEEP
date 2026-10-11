#version 430 core
// dust.vert — poussière en suspension : 4500 points générés sans tampon (gl_VertexID),
// répartis dans un cube de 40 m qui suit la caméra ; visibles seulement près des lumières.
uniform mat4 uVP; uniform vec3 uCam; uniform float uTime; uniform float uPx;
const int MAXL=32; uniform int uNL; uniform vec3 uLPos[MAXL]; uniform vec3 uLCol[MAXL]; uniform float uLR;
out float vB;
float h(float n){ return fract(sin(n*127.1)*43758.5453); }
void main(){
  float f=float(gl_VertexID);
  vec3 base=(vec3(h(f),h(f+11.3),h(f+23.7))*2.0-1.0)*20.0;
  vec3 vel=vec3(h(f+3.1)-0.5,(h(f+5.5)-0.5)*0.4-0.03,h(f+8.8)-0.5)*0.15;
  vec3 turb=vec3(sin(uTime*0.3+f),sin(uTime*0.25+f*1.7),cos(uTime*0.2+f*0.7))*0.35;
  vec3 rel=mod(base+vel*uTime+turb-uCam+20.0,40.0)-20.0;
  vec3 pos=uCam+rel;
  float s=0.0;
  for(int i=0;i<uNL;i++){ vec3 L=uLPos[i]-pos; float d2=dot(L,L); if(d2>=uLR*uLR) continue;
    float w=1.0-d2/(uLR*uLR); s+=w*w/(0.25+d2*0.18)*uLCol[i].r; }
  float tw=0.5+0.5*sin(uTime*(0.8+h(f+1.7))+f*5.0);
  vB=s*0.5*tw*(1.0-smoothstep(12.0,20.0,length(rel)));
  gl_Position=uVP*vec4(pos,1.0);
  gl_PointSize=clamp(uPx*0.012/max(gl_Position.w,0.05),1.0,4.0);
}
