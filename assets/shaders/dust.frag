#version 430 core
// dust.frag — point rond, orangé, d'intensité vB.
in float vB; out vec4 fc;
void main(){ float r=length(gl_PointCoord*2.0-1.0); if(r>1.0) discard; fc=vec4(vec3(1.0,0.72,0.45)*vB*(1.0-r),1.0); }
