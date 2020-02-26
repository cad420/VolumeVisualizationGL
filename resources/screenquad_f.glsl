#version 430 core
layout(location = 0,rgba32f) uniform volatile image2D screenQuadTexture;
out vec4 fragColor;
void main()
{
    fragColor = imageLoad(screenQuadTexture,ivec2(gl_FragCoord)).xyzw;
    //fragColor = vec4(screenCoord.x,screenCoord.y,0,1);
}