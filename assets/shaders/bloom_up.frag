#version 430 core
// bloom_up.frag — agrandissement (filtre tente 3x3), additionné au niveau supérieur.
in vec2 uv; out vec4 fc; uniform sampler2D uTex; uniform vec2 uTexel;
vec3 T(float x,float y){ return texture(uTex,uv+vec2(x,y)*uTexel).rgb; }
void main(){
  vec3 c=T(-1,-1)+T(0,-1)*2.0+T(1,-1)+T(-1,0)*2.0+T(0,0)*4.0+T(1,0)*2.0+T(-1,1)+T(0,1)*2.0+T(1,1);
  fc=vec4(c/16.0,1.0); }
