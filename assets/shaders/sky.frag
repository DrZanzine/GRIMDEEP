#version 430 core
// sky.frag — ciel du crépuscule (camp de base) : dégradé, soleil couchant, étoiles qui
// scintillent, nuages éclairés par le soleil. uAmt fait disparaître le ciel en entrant dans la mine.
in vec2 uv; out vec4 fc;
uniform mat4 uInvVP; uniform vec3 uCam; uniform vec3 uSunDir; uniform float uAmt; uniform float uTime;
float h21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vn2(vec2 x){ vec2 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(h21(i),h21(i+vec2(1,0)),f.x),mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x),f.y); }
float fbm(vec2 p){ float a=0.5,s=0.0; for(int i=0;i<5;i++){ s+=a*vn2(p); p=p*2.03+vec2(1.7,9.2); a*=0.5; } return s; }
void main(){
  vec2 nd=uv*2.0-1.0; vec4 w=uInvVP*vec4(nd,1.0,1.0); vec3 dir=normalize(w.xyz/w.w-uCam); float y=dir.y; vec3 sun=normalize(uSunDir);
  vec3 hor=vec3(0.80,0.30,0.22), mid=vec3(0.26,0.11,0.30), zen=vec3(0.010,0.018,0.085);
  float t=clamp(y,0.0,1.0);
  vec3 col=mix(hor,mid,smoothstep(0.0,0.26,t)); col=mix(col,zen,smoothstep(0.16,0.9,t));
  float sd=max(dot(dir,sun),0.0);
  col+=vec3(1.0,0.55,0.25)*pow(sd,5.0)*0.45+vec3(1.0,0.72,0.42)*pow(sd,48.0)*1.1+vec3(1.0,0.92,0.75)*smoothstep(0.9992,0.9998,sd)*14.0;
  if(y>0.02){                                   // étoiles qui scintillent doucement
    vec2 sp=dir.xz/(y+0.25)*70.0, ci=floor(sp), cf=fract(sp)-0.5; float hh=h21(ci);
    float stl=step(0.962,hh)*smoothstep(0.30,0.0,length(cf-(vec2(h21(ci+7.0),h21(ci+13.0))-0.5)*0.6));
    stl*=0.55+0.45*sin(uTime*(0.8+hh*3.0)+hh*60.0); col+=vec3(0.8,0.86,1.0)*stl*1.4*smoothstep(0.05,0.45,y);
  }
  if(y>0.0){
    vec2 cp=dir.xz/(y+0.18)*0.8+vec2(uTime*0.006,0.0); float cl=fbm(cp*1.5), cl2=fbm(cp*1.5+vec2(0.12,0.05)*3.0);
    float cov=smoothstep(0.50,0.80,cl); float edge=clamp((cl-cl2)*6.0+0.5,0.0,1.0);
    vec3 cc=mix(vec3(0.34,0.20,0.30),vec3(1.35,0.66,0.36),edge*(0.35+0.65*pow(sd,2.0)));
    col=mix(col,cc,cov*0.88*smoothstep(0.0,0.14,y));
  } else col=mix(hor*0.72,vec3(0.20,0.12,0.11),smoothstep(0.0,-0.3,y));
  fc=vec4(col*uAmt,1.0);
}
