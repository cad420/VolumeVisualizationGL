#version 430 core
in vec3 texCoord;

// Multi Render Targets
layout(location = 0) out vec4 entryPos; 
layout(location = 1) out vec4 exitPos;

layout(location = 1) uniform mat4 ModelMatrix;
layout(location = 2) uniform vec3 viewPos;


/*
* State configuration:
* 1.Enable blending
* 2.Set blend function as srcRGB *(1), dstRBG(1), srcAlpha(1), dstAlpha(1)
* 3.Set the polygon front face according to your vertex orderer.
*/
//out vec4 fragColor;
void main() 
{
	vec3 maxPoint = vec3(ModelMatrix*vec4(1));
	vec3 minPoint = vec3(ModelMatrix*vec4(0,0,0,1));

	bool inner = false;
	vec3 eyePos = viewPos;
	if(eyePos.x >= minPoint.x && eyePos.x <= maxPoint.x 
	&& eyePos.y >= minPoint.y && eyePos.y <= maxPoint.y 
	&& eyePos.z >= minPoint.z && eyePos.z <= maxPoint.z)
		inner = true;

	if(gl_FrontFacing)
	{
		entryPos = vec4(texCoord,1.0);
		exitPos = vec4(0,0,0,0);
	}
	else
	{
		exitPos = vec4(texCoord,1.0);
		if(inner){
			eyePos = (eyePos-minPoint)/(maxPoint-minPoint);		// normalized to sample space of [0,1]^3
			entryPos=vec4(eyePos,0);
		}else{
			entryPos=vec4(0,0,0,0);
		}
	}
	return;
}