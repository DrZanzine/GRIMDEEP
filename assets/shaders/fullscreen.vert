#version 430 core
// fullscreen.vert — un seul triangle qui couvre l'écran (sans tampon de sommets) ; uv dans [0,1].
out vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.0-1.0,0.0,1.0); }
