#version 430 core

layout(location = 0) uniform mat4 MVPMatrix;

layout(location = 0) in vec3 VertexPosition;
layout(location = 1) in vec3 TexturePosition;

void main()
{
    gl_Position = MVPMatrix *vec4(VertexPosition,1.0);
}