#version 430 core

layout(location = 0) uniform sampler1D texTransfunc;
layout(location = 1) uniform sampler3D volumeTexture;

layout(location = 2,rgba32f) uniform volatile image2D entryPosTexture;
layout(location = 3,rgba32f) uniform volatile image2D exitPosTexture;
layout(location = 4,rgba32f) uniform volatile image2D resutlTexture;

//layout(location = 0 ) uniform mat4 ModelMatrix;
//layout(location = 1 ) uniform mat4 ViewMatrix;
//layout(location = 2) uniform float step;
//layout(location = 0) uniform vec3 viewPos;

in vec2 screenCoord;
out vec4 fragColor;
float step = 0.01;

void main()
{
	vec3 rayStart = vec3(imageLoad(entryPosTexture,ivec2(gl_FragCoord)).xyz);
	vec3 rayEnd = vec3(imageLoad(exitPosTexture,ivec2(gl_FragCoord)).xyz);

	vec3 start2end = vec3(rayEnd - rayStart);
	vec4 color = imageLoad( resutlTexture, ivec2( gl_FragCoord ) ).xyzw;
	//vec4 color = vec4(0);
	vec4 bg = vec4( 0.f, 0.f, 0.f, .00f );
	vec3 direction = normalize( start2end );
	float distance = dot( direction, start2end );
	int steps = int( distance / step );
	vec3 samplePoint = rayStart;

	for ( int i = 0; i < steps; ++i ) {
		samplePoint += direction * step;
		vec4 scalar = texture(volumeTexture,samplePoint);
		vec4 sampledColor = texture(texTransfunc, scalar.r);
		//sampledColor.a = 1-pow((1-sampledColor.a),correlation[curLod]);
		color = color + sampledColor * vec4( sampledColor.aaa, 1.0 ) * ( 1.0 - color.a );
		if ( color.a > 0.99 ) {
			break;
		}
	}
	color = color + vec4( bg.rgb, 0.0 ) * ( 1.0 - color.a );
	color.a = 1.0;
	//imageStore( exitPosTexture, ivec2( gl_FragCoord ), vec4( samplePoint, 0.0 ) );  // Terminating flag
	fragColor = color;
}