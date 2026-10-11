#version 430 core
// scene.frag — ombrage de toute la scène (HDR, linéaire).
//
// Le numéro de matériau vMat (src/render/materials.h) choisit l'aspect :
//   0 roche (textures triplanaires, strates, humidité, mousse ; 0.2 couloir lisse, 0.4 bloc d'éboulis)
//   1 bois, 2 étoupe goudronnée, 3 toile d'araignée, 4 eau, 5 acier, 6 fourrure, 7 trou noir,
//   8 rouge émissif, 9 cristal, 10 lave, 11 os, 12 champignon, 13 verre de lanterne, 14 étoffe,
//   15 chitine, 16 golem, 17 ciel de la sortie, 18 or, 19 fiole, 20 jaune émissif,
//   21..68 palette des modèles (uMdlA / uMdlE), 90..94 « la Dame », 100 herbe, 120 bois calciné.
//
// Éclairage : jusqu'à 32 lumières ponctuelles (uLPos / uLCol, portée uLR), dont 4 avec ombres
// douces (cube maps uShadow, 8 échantillons) ; diffus + spéculaire Blinn-Phong avec Fresnel ;
// diffusion dans l'air (halo autour des lumières) ; brouillard. Au camp (biome 6) : soleil
// couchant directionnel et brume colorée. Les biomes (uTint, uAmbB, uWetB, uMossB) sont
// interpolés selon vBiome.
in vec3 vPos; in vec3 vN; in vec2 vUV; flat in float vMat; in float vSeed; in float vBiome; in vec3 vLocal;
out vec4 fragColor;
uniform float uGhost;                               // dissolution de « la Dame » (0..1)
uniform vec3 uCam; uniform float uTime;
uniform sampler2D uRockCol, uRockNrm, uWoodCol, uWoodNrm;
uniform samplerCubeArray uShadow;
const int MAXL=32;
uniform int uNL;
uniform vec3 uLPos[MAXL]; uniform vec3 uLCol[MAXL]; uniform int uLSh[MAXL];
uniform float uLR;
const float PI=3.14159265;
uniform vec3 uTint[7]; uniform vec3 uAmbB[7]; uniform float uWetB[7]; uniform float uMossB[7];
uniform vec3 uSunDir; uniform vec3 uSunCol; uniform vec3 uHaze; uniform vec2 uHubC; uniform float uHubG;
uniform vec4 uMdlA[48]; uniform vec4 uMdlE[48];     // matériaux des modèles OBJ : (Kd, Ns) et (Ke, brillance)

// ---- hachage et bruit de valeur 3D (variations procédurales)
float h31(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.zyx+31.32); return fract((p.x+p.y)*p.z); }
float vn(vec3 x){ vec3 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(mix(h31(i),h31(i+vec3(1,0,0)),f.x),mix(h31(i+vec3(0,1,0)),h31(i+vec3(1,1,0)),f.x),f.y),
             mix(mix(h31(i+vec3(0,0,1)),h31(i+vec3(1,0,1)),f.x),mix(h31(i+vec3(0,1,1)),h31(i+vec3(1,1,1)),f.x),f.y),f.z); }
// bruit « interleaved gradient » : rotation aléatoire par pixel des échantillons d'ombre
float ign(vec2 p){ return fract(52.9829189*fract(dot(p,vec2(0.06711056,0.00583715)))); }

// ---- placage triplanaire : texture projetée selon les 3 axes, pondérée par la normale
vec4 tri(sampler2D t, vec3 p, vec3 w, float sc){
  return texture(t,p.zy*sc)*w.x + texture(t,p.xz*sc)*w.y + texture(t,p.xy*sc)*w.z; }
vec3 triN(sampler2D t, vec3 p, vec3 n, vec3 w, float sc){
  vec3 tx=texture(t,p.zy*sc).xyz*2.0-1.0, ty=texture(t,p.xz*sc).xyz*2.0-1.0, tz=texture(t,p.xy*sc).xyz*2.0-1.0;
  tx=vec3(tx.xy+n.zy, abs(tx.z)*n.x);
  ty=vec3(ty.xy+n.xz, abs(ty.z)*n.y);
  tz=vec3(tz.xy+n.xy, abs(tz.z)*n.z);
  return normalize(tx.zyx*w.x + ty.xzy*w.y + tz.xyz*w.z); }

// ---- ondes concentriques des gouttes sur les flaques (perturbation de la normale)
vec2 ripples(vec2 p, float t){
  vec2 g=floor(p/1.5), acc=vec2(0.0);
  for(int j=-1;j<=1;j++) for(int i=-1;i<=1;i++){
    vec2 c=g+vec2(i,j);
    float hh=h31(vec3(c,7.0)); vec2 ctr=(c+vec2(h31(vec3(c,1.0)),h31(vec3(c,2.0))))*1.5;
    float per=2.5+hh*3.0, ph=fract(t/per+hh);
    float r=length(p-ctr), rr=ph*0.7;
    float w=exp(-pow((r-rr)*16.0,2.0))*(1.0-ph);
    acc+=(p-ctr)/max(r,1e-3)*w*sin((r-rr)*45.0);
  } return acc; }

// cristal : facettes nettes, cœur lumineux qui s'intensifie vers la pointe, bandes de croissance, fêlures internes,
// lueur qui dérive lentement, liseré de Fresnel et paillettes scintillantes
void crystalShade(vec3 col, vec3 P, vec3 V, float dP, inout vec3 albedo, inout vec3 emis, inout vec3 N, inout float gloss, inout float shin){
  vec3 fn=normalize(cross(dFdx(P),dFdy(P))); if(dot(fn,V)<0.0) fn=-fn;
  float along=clamp(vUV.y,0.0,1.0), fres=pow(1.0-max(dot(fn,V),0.0),2.5);
  float crack=1.0-smoothstep(0.0,0.07,abs(vn(P*2.2+vec3(along*2.0,0.0,0.0))*2.0-1.0));
  float inner=0.35+0.65*vn(P*1.4+vec3(0.0,uTime*0.12,0.0)), bands=0.5+0.5*sin(along*22.0+vn(P*3.0)*5.0), core=pow(along,1.4);
  albedo=col*0.04;
  emis=col*(0.18+0.95*core)*(0.55+0.55*inner)*(0.82+0.3*bands)+mix(col,vec3(1.0),0.5)*crack*0.7*(0.4+core)+mix(col,vec3(1.0),0.7)*fres*1.3;
  float gl=h31(floor(P*38.0)); emis+=vec3(2.0)*step(0.992,gl)*(0.6+0.4*sin(uTime*5.0+gl*40.0))*(1.0-smoothstep(10.0,40.0,dP));
  N=fn; gloss=1.6; shin=220.0;
}

// ---- ombre douce : 8 échantillons en spirale dans la cube map de la lumière (1 = éclairé)
float shadowAt(int slot, vec3 P, vec3 Ng, vec3 lp){
  vec3 dir=(P+Ng*0.05)-lp; float dist=length(dir); vec3 d=dir/dist;
  vec3 t1=normalize(cross(d,abs(d.y)<0.9?vec3(0,1,0):vec3(1,0,0))), t2=cross(d,t1);
  float rot=ign(gl_FragCoord.xy)*6.2831853, bias=0.05+0.022*dist, lit=0.0;
  for(int n=0;n<8;n++){
    float r=sqrt((float(n)+0.5)/8.0), a=float(n)*2.39996+rot;
    vec3 off=(t1*cos(a)+t2*sin(a))*r*0.02;
    lit+= (dist-bias > texture(uShadow, vec4(d+off,float(slot))).r) ? 0.0 : 1.0;
  } return lit/8.0; }

// =============================================================================
void main(){
  vec3 P=vPos; vec3 Ng=normalize(vN); vec3 V=normalize(uCam-P);
  vec3 rd=P-uCam; float dP=length(rd); vec3 vd=rd/dP;
  if(dot(Ng,V)<0.0) Ng=-Ng;
  float bfv=clamp(vBiome,0.0,6.0); int bi0=int(floor(bfv)); int bi1=min(bi0+1,6); float bt=fract(bfv); float hubW=smoothstep(5.5,6.0,bfv);
  vec3 tint=mix(uTint[bi0],uTint[bi1],bt); vec3 ambB=mix(uAmbB[bi0],uAmbB[bi1],bt); float wetB=mix(uWetB[bi0],uWetB[bi1],bt); float mossB=mix(uMossB[bi0],uMossB[bi1],bt);

  // ---------- eau : reflets spéculaires des torches, absorption
  if(vMat>3.5 && vMat<4.5){
    vec2 rg=ripples(P.xz,uTime)*0.045+vec2(vn(vec3(P.xz*7.0,uTime*0.35))-0.5,vn(vec3(P.zx*7.0+7.0,uTime*0.35))-0.5)*0.012;
    vec3 Nw=normalize(vec3(rg.x,1.0,rg.y));
    float ndv=max(dot(Nw,V),0.0); float fres=0.03+0.97*pow(1.0-ndv,5.0);
    vec3 refl=vec3(0.0035,0.0035,0.004)*fres*3.0;
    for(int i=0;i<uNL;i++){
      vec3 Lv=uLPos[i]-P; float d2=dot(Lv,Lv); if(d2>=uLR*uLR) continue;
      float d=sqrt(d2); vec3 L=Lv/d; float win=1.0-d2/(uLR*uLR); win*=win;
      vec3 H=normalize(L+V); float nh=max(dot(Nw,H),0.0);
      float s=pow(nh,1400.0)*22.0+pow(nh,70.0)*0.7;
      vec3 c=uLCol[i]*(win/(0.25+d2*0.26))*s*(0.25+fres);
      if(uLSh[i]>=0 && s>0.01) c*=shadowAt(uLSh[i],P,vec3(0,1,0),uLPos[i]);
      refl+=c;
    }
    float edge=1.0-smoothstep(0.65,1.0,vUV.x);
    fragColor=vec4(refl*edge*exp(-dP*0.012), 0.66*edge);
    return;
  }

  if(vMat>9.5 && vMat<10.5){                    // lave : émissive, croûte sombre
    float fl=vn(vec3(P.x*1.3,uTime*0.04,P.z*1.3))*0.6+vn(vec3(P.x*4.0,0.0,P.z*4.0+uTime*0.05))*0.4;
    float crack=smoothstep(0.38,0.62,fl);
    vec3 lav=mix(vec3(0.04,0.006,0.0),vec3(1.0,0.26,0.04),crack)*(0.9+0.2*vn(vec3(P.xz*9.0,uTime*0.1)));
    fragColor=vec4(lav*exp(-dP*0.012),1.0); return;
  }
  vec3 albedo=vec3(0.5), N=Ng, emis=vec3(0.0); float gloss=0.2, shin=16.0, alphaOut=1.0; bool thin=false;
  float flick=0.8+0.2*vn(vec3(uTime*9.0,vSeed,1.0));

  if(vMat<0.5){
    vec3 w=pow(abs(Ng),vec3(6.0)); w/=dot(w,vec3(1.0));
    vec4 c0=tri(uRockCol,P,w,0.27), c1=tri(uRockCol,P,w,1.1); float farK=smoothstep(25.0,110.0,dP);
    float fb=smoothstep(26.0,80.0,dP)*hubW;       // montagne vue de loin : motifs 6 fois plus grands (plus de motif d'écailles répétitif)
    if(fb>0.001){ c0=mix(c0,tri(uRockCol,P,w,0.045),fb); c1=mix(c1,tri(uRockCol,P,w,0.18),fb); }
    albedo=c0.rgb*(0.55+0.9*mix(c1.rgb,vec3(0.5),farK*0.5)); gloss=c0.a*(1.0-0.85*farK);
    { vec3 fn=normalize(cross(dFdx(P),dFdy(P))); if(dot(fn,Ng)<0.0) fn=-fn; Ng=normalize(mix(Ng,fn,0.35)); w=pow(abs(Ng),vec3(6.0)); w/=dot(w,vec3(1.0)); }
    float strata=vn(vec3(P.x*0.08,P.y*4.5,P.z*0.08));
    albedo*=mix(0.75,1.2,strata); albedo*=tint; albedo*=0.82+0.36*vn(P*0.045+vec3(3.0,8.0,1.0));
    if(hubW>0.01){                                // montagne du camp : herbe et mousse au pied, neige sur les hauteurs
      float hy=P.y-uHubG, nh=vn(P*0.35)-0.5;
      float gr=(1.0-smoothstep(1.5,9.0,hy+nh*6.0))*smoothstep(0.25,0.65,Ng.y);
      albedo=mix(albedo,vec3(0.10,0.22,0.06)*(0.7+0.6*vn(P*3.0)),gr*hubW*0.85);
      float sn=smoothstep(38.0,62.0,hy+nh*14.0)*smoothstep(0.32,0.62,Ng.y);
      albedo=mix(albedo,vec3(0.78,0.80,0.92),sn*hubW*0.9); gloss*=1.0-0.6*sn*hubW;
    }
    float mossM=mossB*smoothstep(0.42,0.62,vn(P*0.8+vec3(5.0,2.0,9.0)))*(0.35+0.65*smoothstep(-0.3,0.9,Ng.y));
    albedo=mix(albedo,vec3(0.06,0.14,0.05)*(0.6+0.8*vn(P*6.0)),mossM*0.85);
    vec3 n0=triN(uRockNrm,P,Ng,w,0.27), n1=triN(uRockNrm,P,n0,w,1.1);
    if(fb>0.001){ vec3 m0=triN(uRockNrm,P,Ng,w,0.045), m1=triN(uRockNrm,P,m0,w,0.18); n0=normalize(mix(n0,m0,fb)); n1=normalize(mix(n1,m1,fb)); }
    N=normalize(mix(n0,n1,0.55)); N=normalize(mix(N,Ng,smoothstep(35.0,150.0,dP)*0.8));
    float fl=smoothstep(0.6,0.85,Ng.y);
    albedo=mix(albedo,albedo*vec3(1.0,0.92,0.8)*0.85,fl);
    if(vMat>0.3){ float bs=h31(vec3(vSeed*3.1,1.7,2.3)); albedo*=vec3(1.18,1.08,0.95)*(0.8+0.5*bs); gloss*=0.7; }
    float wm=vn(P*0.21+vec3(3.0,1.0,7.0))*0.65+vn(P*0.7)*0.35;
    float wthr=0.66-0.32*(wetB-0.5); float wet=smoothstep(wthr-0.08,wthr+0.08,wm);
    albedo*=mix(1.0,0.5,wet);
    shin=mix(9.0,80.0,wet); gloss=mix(gloss*0.18,0.8,wet);
    if(vMat>0.1 && vMat<0.3){                     // couloir des catacombes : pierre lisse, sombre, coulures verticales luisantes
      float kc=clamp(vSeed,0.0,1.0), streak=smoothstep(0.5,0.9,vn(vec3(P.x*6.0,P.y*0.35,P.z*6.0)));
      vec3 cAlb=vec3(0.085,0.09,0.088)*(0.8+0.35*vn(P*vec3(0.5,0.05,0.5)))*(1.0-0.45*streak);
      albedo=mix(albedo,cAlb,kc); gloss=mix(gloss,0.45+0.5*streak,kc); shin=mix(shin,70.0,kc); N=normalize(mix(N,Ng,kc*0.85));
    }
  } else if(vMat<2.5){
    vec2 uv=vUV*vec2(0.8,1.9);
    vec4 wc=texture(uWoodCol,uv); vec3 tn=texture(uWoodNrm,uv).xyz*2.0-1.0;
    vec3 dp1=dFdx(P), dp2=dFdy(P); vec2 du1=dFdx(uv), du2=dFdy(uv);
    vec3 d2p=cross(dp2,Ng), d1p=cross(Ng,dp1);
    vec3 T=d2p*du1.x+d1p*du2.x, B=d2p*du1.y+d1p*du2.y;
    float im=inversesqrt(max(max(dot(T,T),dot(B,B)),1e-12));
    N=normalize(mat3(T*im,B*im,Ng)*tn);
    albedo=wc.rgb*mix(1.0,0.75,vn(P*5.0)); gloss=wc.a; shin=22.0;
    if(vMat>1.5){ float g=smoothstep(0.1,0.9,vn(P*30.0+uTime*0.5)); albedo*=0.05; gloss=0.02; emis=vec3(1.0,0.3,0.06)*g*0.35*flick; }
  } else if(vMat<3.5){                       // toile d'araignée
    vec2 p=vUV; float r=length(p); float ang=atan(p.y,p.x)/1.5707963; float fa=ang*5.0;
    float aa=fwidth(r)*0.8+1e-4, wuv=0.0045/max(vSeed,0.2);
    float sp=abs(fract(fa+0.5)-0.5)/5.0*1.5707963*r;
    float rr=r+0.045*sin(fract(fa)*3.1416);
    float dr=abs(fract(rr*9.0+0.5)-0.5)/9.0;
    float spokes=(1.0-smoothstep(wuv,wuv+aa,sp))*step(0.02,r)*step(vn(vec3(p*4.0+3.0,vSeed)),0.9);
    float rings=(1.0-smoothstep(wuv*0.8,wuv*0.8+aa,dr))*step(0.1,r)*step(vn(vec3(p*7.0,vSeed*3.0)),0.72);
    float cov=max(spokes,rings)*(1.0-smoothstep(0.88,1.0,p.x+p.y));
    if(cov<0.02) discard;
    albedo=vec3(0.8,0.8,0.82); gloss=1.0; shin=60.0; alphaOut=cov; thin=true;
  } else if(vMat<5.5){                       // acier
    albedo=vec3(0.22,0.23,0.26)*(0.8+0.4*vn(P*40.0)); gloss=0.3; shin=60.0;
  } else if(vMat<6.5){                       // fourrure
    float tail=step(0.5,vSeed)*step(vSeed,1.5);
    albedo=mix(vec3(0.20,0.16,0.13)*(0.7+0.6*vn(P*70.0)), vec3(0.34,0.2,0.18), tail+step(0.5,vSeed)*step(vSeed,0.8));
    gloss=0.25; shin=28.0;
  } else if(vMat<7.5){                       // intérieur d'un trou
    albedo=vec3(0.0); gloss=0.0; shin=8.0;
  } else if(vMat<8.5){ albedo=vec3(0.0); emis=vec3(0.9,0.22,0.1)*0.7; gloss=1.0; shin=200.0;
  } else if(vMat<9.5){                       // cristal luminescent
    vec3 cc=mix(vec3(0.65,0.2,1.0),vec3(0.2,0.7,1.0),step(0.5,vSeed));
    crystalShade(cc*1.3,P,V,dP,albedo,emis,N,gloss,shin);
  } else if(vMat<11.5){                      // os
    albedo=vec3(0.62,0.58,0.47)*(0.75+0.35*vn(P*25.0)); gloss=0.25; shin=26.0;
  } else if(vMat<12.5){                      // champignon luminescent
    if(vSeed>0.5){ albedo=vec3(0.02,0.08,0.1); emis=vec3(0.1,0.9,1.0)*(0.9+0.4*vn(P*20.0)); gloss=0.5; shin=40.0; } else { albedo=vec3(0.55,0.6,0.58); gloss=0.2; shin=20.0; }
  } else if(vMat<13.5){                      // verre de lanterne
    albedo=vec3(0.0); emis=vec3(1.0,0.78,0.5)*2.4; gloss=0.0;
  } else if(vMat<14.5){                      // étoffe sombre
    albedo=vec3(0.07,0.06,0.06)*(0.6+0.8*vn(P*18.0)); gloss=0.08; shin=10.0;
  } else if(vMat<15.5){                      // chitine
    albedo=vec3(0.035,0.03,0.03); gloss=1.0; shin=90.0;
  } else if(vMat<16.5){                      // golem : roche et veines de lave
    float cr=smoothstep(0.52,0.62,vn(P*5.0)); albedo=vec3(0.1,0.085,0.075); emis=vec3(1.2,0.28,0.04)*cr; gloss=0.2; shin=20.0;
  } else if(vMat<17.5){                      // ciel de la sortie
    albedo=vec3(0.0); emis=vec3(0.85,0.95,1.2)*1.5; gloss=0.0;
  } else if(vMat<18.5){                      // or
    albedo=vec3(0.9,0.62,0.12); emis=vec3(0.22,0.14,0.02); gloss=1.5; shin=60.0;
  } else if(vMat<19.5){                      // fiole
    albedo=vec3(0.1,0.0,0.0); emis=vec3(1.0,0.08,0.1)*1.2; gloss=1.0; shin=80.0;
  } else if(vMat>89.5 && vMat<95.0){        // « la Dame » : peau, robe, cheveux, yeux, orbites
    vec3 L=vLocal;
    if(uGhost>0.0 && vn(L*13.0)*0.8+vn(L*41.0)*0.2 < uGhost*1.15-0.08) discard;
    if(vMat<90.5){                               // peau marbrée, veines violacées, crasse, plaies, pores
      float mott=vn(L*7.0), vein=(1.0-smoothstep(0.0,0.07,abs(vn(L*vec3(11.0,5.0,11.0))*2.0-1.0)))*smoothstep(0.35,0.7,vn(L*3.0+5.0));
      float vein2=1.0-smoothstep(0.0,0.05,abs(vn(L*vec3(23.0,9.0,23.0)+3.0)*2.0-1.0));
      albedo=vec3(0.50,0.52,0.56)*(0.72+0.4*mott);
      albedo=mix(albedo,vec3(0.17,0.08,0.20),clamp(vein*0.75+vein2*0.35,0.0,1.0));
      albedo=mix(albedo,vec3(0.24,0.22,0.19),smoothstep(0.55,0.85,vn(L*2.4+9.0))*0.55);
      albedo=mix(albedo,vec3(0.30,0.04,0.04),smoothstep(0.78,0.9,vn(L*4.0+21.0))*0.6);
      vec3 bump=vec3(vn(L*95.0),vn(L*95.0+3.1),vn(L*95.0+7.7))-0.5; N=normalize(N+bump*0.32*(1.0-smoothstep(4.0,12.0,dP)));
      gloss=0.4; shin=45.0;
    } else if(vMat<91.5){                        // robe : toile sale, déchirée, trouée, tachée
      float hem=vUV.y, tear=0.04+0.24*vn(vec3(vUV.x*11.0,1.0,2.0))+0.12*vn(vec3(vUV.x*37.0,3.0,1.0));
      if(hem<tear) discard;
      if(hem<1.5 && vn(L*vec3(8.0,5.0,8.0)+11.0)>0.8) discard;
      float weave=mix(1.0,0.5+0.5*sin(vUV.x*1400.0)*sin(hem*700.0),1.0-smoothstep(2.0,8.0,dP));
      albedo=vec3(0.66,0.64,0.58)*(0.78+0.3*vn(L*vec3(26.0,3.0,26.0)))*(0.9+0.1*weave);
      float dirt=smoothstep(0.5,0.0,hem-tear)*0.75+smoothstep(0.55,0.78,vn(L*3.0))*0.5;
      albedo=mix(albedo,vec3(0.16,0.12,0.09),clamp(dirt,0.0,0.85));
      albedo=mix(albedo,vec3(0.22,0.02,0.02),smoothstep(0.72,0.8,vn(L*4.5+9.0))*0.85);
      gloss=0.05; shin=8.0; thin=true;
    } else if(vMat<92.5){ albedo=vec3(0.010,0.009,0.011); gloss=0.7; shin=70.0;
    } else if(vMat<93.5){ albedo=vec3(0.0); emis=vec3(5.0,0.22,0.1)*(0.85+0.15*sin(uTime*3.3)); gloss=0.0;
    } else { albedo=vec3(0.008,0.0,0.0); emis=vec3(0.06,0.0,0.0); gloss=0.3; shin=30.0; }
  } else if(vMat>119.5){                     // bois calciné : noir, quelques braises qui s'éteignent (vSeed = 20 + intensité*10)
    float amount=clamp((vSeed-20.0)/10.0,0.0,1.0); float g=smoothstep(0.58,0.9,vn(P*22.0+vec3(0.0,uTime*0.25,0.0)));
    albedo=vec3(0.028,0.022,0.018)*(0.6+0.8*vn(P*25.0)); gloss=0.05; shin=8.0; emis=vec3(1.0,0.26,0.05)*g*amount*0.9*flick;
  } else if(vMat>99.5){                      // herbe du camp de base, chemin de terre
    vec3 g1=vec3(0.10,0.22,0.055), g2=vec3(0.23,0.36,0.10), g3=vec3(0.33,0.30,0.12), dirt=vec3(0.30,0.21,0.13);
    float n1=vn(vec3(P.xz*0.32,1.0)), n2=vn(vec3(P.xz*2.1,3.0)), n3=vn(vec3(P.xz*8.0,5.0));
    albedo=mix(mix(g1,g2,n1),g3,smoothstep(0.62,0.9,vn(vec3(P.xz*0.11,9.0)))*0.6)*(0.78+0.4*n2)*(0.86+0.28*n3);
    float pathX=3.2*sin(P.z*0.05)+1.2*sin(P.z*0.13);
    float path=1.0-smoothstep(1.3,2.7,abs(P.x-pathX)); path*=smoothstep(2.0,6.0,P.z-uHubC.y+60.0);
    float plaza=1.0-smoothstep(11.0,19.0,length(P.xz-uHubC));
    float dirtK=max(path*0.9,plaza)*(0.75+0.25*n2);
    albedo=mix(albedo,dirt*(0.8+0.5*n3),dirtK);
    N=normalize(Ng+vec3(n2-0.5,0.0,n3-0.5)*0.35); gloss=0.04; shin=6.0;
  } else if(vMat>20.5){                      // matériau de modèle OBJ (couleur / émission du .mtl)
    int mi=clamp(int(vMat-20.5),0,47); albedo=uMdlA[mi].rgb; shin=max(uMdlA[mi].a,4.0); emis=uMdlE[mi].rgb*0.6; gloss=uMdlE[mi].a;
    if(uMdlE[mi].a<-0.5) crystalShade(uMdlE[mi].rgb*0.8,P,V,dP,albedo,emis,N,gloss,shin);
  } else { albedo=vec3(0.0); emis=vec3(1.0,0.82,0.2)*2.2; gloss=0.0; }

  // ---------- éclairage : somme des lumières ponctuelles (diffus, spéculaire, diffusion dans l'air)
  vec3 diffA=vec3(0), specA=vec3(0), indir=vec3(0), scat=vec3(0);
  for(int i=0;i<uNL;i++){
    vec3 lp=uLPos[i], lc=uLCol[i];
    vec3 oc=lp-uCam; float t0=dot(oc,vd); float a=sqrt(max(dot(oc,oc)-t0*t0,0.05));
    scat+=lc*((atan((dP-t0)/a)-atan(-t0/a))/a);
    vec3 Lv=lp-P; float d2=dot(Lv,Lv);
    if(d2>=uLR*uLR) continue;
    float d=sqrt(d2); vec3 L=Lv/d;
    float win=1.0-d2/(uLR*uLR); win*=win;
    vec3 c=lc*(win/(0.25+d2*0.26));
    indir+=c*0.025;
    float ng=dot(Ng,L); if(thin) ng=abs(ng);
    if(ng<=0.0) continue;
    float sh=1.0; if(uLSh[i]>=0) sh=shadowAt(uLSh[i],P,Ng,lp);
    if(sh<=0.0) continue;
    float ndl=dot(N,L); if(thin) ndl=abs(ndl); ndl=max(ndl,0.0);
    diffA+=c*ndl*sh;
    vec3 H=normalize(L+V); float nh=max(dot(N,H),0.0);
    float fres=0.04+0.96*pow(1.0-max(dot(H,V),0.0),5.0);
    specA+=c*sh*ndl*pow(nh,shin)*(shin+8.0)/(8.0*PI)*mix(0.15,1.0,fres)*gloss;
  }
  // ---------- ambiance, soleil du camp, brouillard
  vec3 amb=ambB*(0.4+0.6*Ng.y);
  if(hubW>0.0){                               // soleil couchant (sans ombre portée) : lumière directionnelle chaude
    float sn=max(dot(N,uSunDir),0.0), wrap=max(dot(N,uSunDir)*0.5+0.5,0.0);
    diffA+=uSunCol*(sn*0.85+wrap*0.15)*hubW;
    vec3 Hs=normalize(uSunDir+V); specA+=uSunCol*pow(max(dot(N,Hs),0.0),shin)*(shin+8.0)/(8.0*PI)*gloss*0.35*sn*hubW;
  }
  vec3 col=albedo*(diffA+indir+amb)+specA+emis;
  float fogC=exp(-dP*0.012), fogH=exp(-dP*0.0026);
  col=mix(col*fogC+scat*0.0016, mix(uHaze,col,fogH)+scat*0.0011, hubW);
  fragColor=vec4(col,alphaOut);
}
