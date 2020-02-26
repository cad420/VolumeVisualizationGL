#version 430 core
const vec2 screenQuadVertexCoord[4]={vec2(-1.0,-1.0),vec2(1.0,-1.0),vec2(-1.0,1.0),vec2(1.0,1.0)};
const vec2 screenQuadTexCoord[4]={vec2(0,0),vec2(1,0),vec2(0,1.0),vec2(1.0,1.0)};
out vec2 screenCoord;
void main()
{
    gl_Position = vec4(screenQuadVertexCoord[gl_VertexID].x,screenQuadVertexCoord[gl_VertexID].y,0.0,1.0);
    screenCoord = screenQuadTexCoord[gl_VertexID];
}