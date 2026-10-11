#version 430 core
// particles.vert — particules en points : aPS = (position, taille en m), aK = (type, graine, vie, intensité).
// La taille à l'écran dépend de la distance (uPx = conversion mètres -> pixels).
layout(location=0) in vec4 aPS; layout(location=1) in vec4 aK;
uniform mat4 uVP; uniform float uPx; out vec4 vK;
void main(){ gl_Position=uVP*vec4(aPS.xyz,1.0); gl_PointSize=clamp(aPS.w*uPx/max(gl_Position.w,0.05),1.0,500.0); vK=aK; }
