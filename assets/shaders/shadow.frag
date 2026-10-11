#version 430 core
// shadow.frag — écrit la distance à la lumière (cube map d'ombre R32F).
in vec3 vWP; uniform vec3 uLP; out float outD;
void main(){ outD=length(vWP-uLP); }
