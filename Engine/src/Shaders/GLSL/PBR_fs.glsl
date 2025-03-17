#version 330 core

layout(location = 0) out vec4 outColor;

precision highp float;

in vec3 normalInterp;
in vec3 vertPos;
in vec3 camPos;

const vec3 lightPos = vec3(1.25, 1.0, 2.0);


layout (std140) uniform PBR
{
	vec3 base_color;
	float roughness;
	float metallic;
    float specular;
};



#define PI 3.1415926535897932384626433832795

vec3 lin2rgb(vec3 lin) {
	return pow(lin, vec3(1.0/2.2));
}

vec3 rgb2lin(vec3 rgb) {
	return pow(rgb, vec3(2.2));
}

// normal distribution function
// GGX/Trowbridge-reitz model
float distributionGGX(float NdotH, float roughness)
{
    float a = pow(roughness, 2.0);
    float a2 = pow(a, 2.0);
    float denom = pow(NdotH, 2.0) * (a2-1.0) + 1.0;
    denom = PI * pow(denom, 2.0);
    return a2/max(denom, 0.0000001);

}

// geometry shadowing function
//schlick-ggx
float geometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = pow(r, 2.0) / 8.0;
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1*ggx2;
}

// fresnel function
vec3 fresnel(float HdotV, vec3 baseReflectivity)
{
    return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0-HdotV, 5.0);
}



void main() {

	vec3 N = normalize(normalInterp);
    vec3 V = normalize(camPos - vertPos);

    vec3 baseReflectivity = mix(vec3(0.04), rgb2lin(base_color), metallic);

    vec3 L = normalize(lightPos - vertPos);
    vec3 H = normalize(V+L);
    
    float distance = length(lightPos - vertPos);
    float attenuation = 1.0 / pow(distance, 2.0);
    vec3 radiance = vec3(1.0) * attenuation;

    float NdotV = max(dot(N, V), 0.0000001);
    float NdotL = max(dot(N, L), 0.0000001);
    float HdotV = max(dot(H, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnel(HdotV, baseReflectivity);

    vec3 spec = D * G * F;
    spec /= 4.0 * NdotV * NdotL;

    vec3 kD = vec3(1.0) - F;

    kD *= 1.0 - metallic;

    vec3 Lo = vec3(0.0);
    Lo += (kD * lin2rgb(base_color) / PI + spec) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * base_color;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // gamma correction
    color = lin2rgb(color);


  outColor = vec4(color, 1.0);
}