#version 430 core
layout(location = 0) uniform mat4 MVPMatrix;

layout(location = 0) in vec3 VertexPosition;
layout(location = 1) in vec3 TexturePosition;

out vec3 texCoord;

void main()
{
	gl_Position = MVPMatrix * vec4(VertexPosition,1.0);
	texCoord = TexturePosition;            //(0,0,0)<--->(1,1,1)
}