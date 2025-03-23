#version 420 core

layout(location = 0) out vec4 outColor;

in vec4 v_Albedo;
in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform int u_DebugMode = 0; // 0=Normal, 1=BaseColor, 2=Normals, 3=ID-based
uniform float u_Time = 0.0;   // For animation effects in debug modes

uniform sampler2D u_albedoMap;

void main()
{
	vec4 finalColor = v_Albedo;
	
	// Additional debug visualizations that need fragment shader processing
	if (u_DebugMode == 2) {
		// Normal visualization with proper normalization
		vec3 normal = normalize(v_Normal);
		finalColor = vec4(normal * 0.5 + 0.5, 1.0);
	}
	else if (u_DebugMode == 4) {
		// Visualization showing pixel position as colors
		finalColor = vec4(
			fract(v_Position.x * 0.1),
			fract(v_Position.y * 0.1),
			fract(v_Position.z * 0.1),
			1.0
		);
	}
	else if (u_DebugMode == 5) {
		// Animated checkerboard pattern for UV debugging
		vec2 coord = gl_FragCoord.xy / 100.0;
		float checker = mod(floor(coord.x) + floor(coord.y), 2.0);
		float t = sin(u_Time) * 0.5 + 0.5; // Oscillate between 0 and 1
		finalColor = mix(
			vec4(1.0, 0.0, 0.0, 1.0),
			vec4(0.0, 1.0, 0.0, 1.0),
			checker > 0.5 ? t : 1.0 - t
		);
	}
	
	outColor = v_Albedo;
}