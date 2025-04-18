#version 420 core

layout(location = 0) out vec4 outColor;

in vec4 v_Albedo;
in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;


layout(binding = 0) uniform sampler2D u_AlbedoMap;    // ALBEDO=0

uniform bool u_HasAlbedoMap = false;

layout (std140, binding=4) uniform SOLID
{
	vec4 color;
};


void main()
{
	vec4 finalColor = v_Albedo;
	

    if (u_HasAlbedoMap) {
        outColor = texture(u_AlbedoMap, v_TexCoord);
    } else {
        outColor = color;
    }
	
}