#version 430 core
// shadow.vert — passe d'ombre : position monde transmise au fragment shader.
layout(location=0) in vec3 aPos; uniform mat4 uVP; out vec3 vWP;
void main(){ vWP=aPos; gl_Position=uVP*vec4(aPos,1.0); }
