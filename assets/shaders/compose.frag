#version 430 core
// compose.frag — image finale : scène + bloom, vignettage, tonemapping ACES, ombres bleutées,
// correction gamma et grain de film.
in vec2 uv; out vec4 fc; uniform sampler2D uHdr, uBloom; uniform float uTime, uAspect, uBloomK;
float hash(vec2 p){ return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453); }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){
  vec3 c=texture(uHdr,uv).rgb+texture(uBloom,uv).rgb*uBloomK;
  vec2 d=(uv-0.5)*vec2(uAspect,1.0); c*=mix(1.0,smoothstep(1.15,0.25,length(d)),0.85);
  c=aces(c);
  float l=dot(c,vec3(0.299,0.587,0.114));
  c=mix(c,c*vec3(0.88,0.98,1.12),(1.0-smoothstep(0.0,0.5,l))*0.6);
  c=pow(c,vec3(1.0/2.2));
  c+=(hash(uv*vec2(1920.0,1080.0)+fract(uTime))-0.5)*0.02;
  fc=vec4(c,1.0); }
