#version 430 core
// particles.frag — aspect des particules selon le type (src/entities/particles.h) :
// 0 flamme, 1 halo, 2 braise, 3 poussière, 4-5 étincelle / goutte, 6 cendre, 7 feu d'explosion,
// 8 fumée (alpha prémultiplié). uFlame = 13 couleurs (torches des biomes, cristaux, yeux de la Dame).
in vec4 vK; uniform float uTime; uniform vec3 uFlame[13]; out vec4 fc;
const float PI_=3.14159265;
float h21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vn2(vec2 x){ vec2 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(h21(i),h21(i+vec2(1,0)),f.x),mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x),f.y); }
void main(){
  float kind=vK.x, seed=vK.y, life=vK.z, inten=vK.w;
  vec2 q=vec2(gl_PointCoord.x*2.0-1.0, 1.0-gl_PointCoord.y*2.0);
  if(kind<0.5){
    float t=clamp((q.y+0.72)/1.6,0.0,1.0);
    float sway=(vn2(vec2(q.y*2.2-uTime*2.0+seed*10.0,seed*7.0))-0.5)*0.55*t + 0.1*sin(uTime*3.0+seed*20.0)*t;
    float width=0.40*sin(PI_*pow(t,0.45))*(1.0-0.25*t);
    float dx=abs(q.x-sway);
    float body=1.0-smoothstep(width*0.5,width,dx);
    float core=1.0-smoothstep(0.0,width*0.5,dx);
    body*=0.75+0.5*vn2(q*3.5+vec2(0.0,-uTime*3.0+seed*5.0));
    body*=step(0.001,width);
    int bi=clamp(int(floor(life+0.5)),0,12); vec3 mid=uFlame[bi]; vec3 hot=mix(mid,vec3(1.0),0.6); vec3 cool=mid*vec3(0.55,0.3,0.25);
    vec3 col=mix(mix(hot,mid,smoothstep(0.0,0.5,t)),cool,smoothstep(0.45,1.0,t));
    col=mix(col,hot*1.5,core*(1.0-t));
    fc=vec4(col*body*inten*1.7,1.0);
  } else if(kind<1.5){
    float r2=dot(q,q); if(r2>1.0) discard;
    int bi=clamp(int(floor(life+0.5)),0,12); fc=vec4(uFlame[bi]*exp(-r2*7.0)*inten*0.4,1.0);
  } else if(kind<2.5){
    float r=length(q); if(r>1.0) discard;
    float a=smoothstep(1.0,0.15,r);
    fc=vec4(mix(uFlame[clamp(int(inten+0.5),0,12)],vec3(1.0),0.25)*a*life*(0.6+0.8*h21(vec2(seed,floor(uTime*12.0))))*4.0,1.0);
  } else if(kind>6.5 && kind<7.5){              // feu d'explosion : blanc -> jaune -> orange -> rouge sombre, bords déchiquetés
    float r=length(q); float n=vn2(q*2.5+seed*3.0+vec2(0.0,uTime*1.5))*0.6+vn2(q*5.0-seed)*0.4;
    float a=smoothstep(1.0,0.15,r+(n-0.5)*0.7); if(a<=0.0) discard;
    vec3 c=life>0.75?mix(vec3(1.0,0.72,0.32),vec3(1.0,0.95,0.8),max(0.0,life-0.88)*8.0):(life>0.4?mix(vec3(1.0,0.32,0.05),vec3(1.0,0.72,0.32),(life-0.4)/0.35):mix(vec3(0.22,0.025,0.0),vec3(1.0,0.32,0.05),life/0.4));
    fc=vec4(c*a*inten*(0.35+life*1.1),1.0);
  } else if(kind>7.5){                          // fumée noire (alpha prémultiplié), rougeoyante quand elle est jeune
    float r=length(q); float n=0.5+0.9*vn2(q*2.2+seed*5.0+vec2(0.0,uTime*0.3));
    float a=smoothstep(1.0,0.0,r)*n*min(1.0,(1.0-life)*6.0)*min(1.0,life*2.5)*1.1; if(a<=0.002) discard; a=min(a,0.95);
    vec3 c=vec3(0.045,0.04,0.037)+vec3(1.0,0.38,0.08)*inten*max(0.0,life-0.72)*2.5;
    fc=vec4(c*a,a);
  } else {
    float r=length(q); if(r>1.0) discard;
    float n=0.6+0.8*vn2(q*3.0+seed*7.0); float a=pow(1.0-r,2.0)*n;
    if(kind>5.5){ fc=vec4(vec3(0.34,0.33,0.33)*a*life*inten*1.3,1.0); }
    else if(kind>3.5){ vec2 qq=q*vec2(1.7,0.55); fc=vec4(vec3(0.7,0.85,1.0)*exp(-dot(qq,qq)*3.0)*inten,1.0); } else fc=vec4(vec3(0.62,0.5,0.38)*a*life*inten,1.0);
  }
}
