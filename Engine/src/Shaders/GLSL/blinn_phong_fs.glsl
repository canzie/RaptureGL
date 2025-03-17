#version 330 core

layout(location = 0) out vec4 outColor;

precision mediump float;

in vec3 normalInterp;
in vec3 vertPos;

const vec3 lightPos = vec3(0.5, 0.8, 0.0);


layout (std140) uniform Phong
{
	vec4 ambientLight;
	vec4 diffuseColor;
	vec4 specularColor;
	float flux;
	float shininess;
};

#define PI 3.1415926535897932384626433832795

vec3 lin2rgb(vec3 lin) {
	return pow(lin, vec3(1.0/2.2));
}

vec3 rgb2lin(vec3 rgb) {
	return pow(rgb, vec3(2.2));
}

vec3 phongBRDF(vec3 lightDir, vec3 viewDir, vec3 normal, 
				vec3 pDiffuseCol, vec3 pSpecularCol, 
				float pShininess)
{
	vec3 color = pDiffuseCol;
	vec3 reflectDir = reflect(-lightDir, normal);
	float specDot = max(dot(viewDir, reflectDir), 0.0);
	
	color += pow(specDot, pShininess) * pSpecularCol;
	return color;
}

void main() {

	vec3 lightDir = lightPos - vertPos;
	float r = length(lightDir);
	lightDir = normalize(lightDir);
	vec3 viewDir = normalize(vec3(0.0)-vertPos);

	vec3 n = normalize(normalInterp);
	vec3 radiance = rgb2lin(diffuseColor.rgb) * rgb2lin(ambientLight.rgb);
	float irradiance = max(dot(lightDir, n), 0.0) * flux / (4.0 * PI * r * r);

	vec3 brdf = phongBRDF(lightDir, viewDir, n, rgb2lin(diffuseColor.rgb), rgb2lin(specularColor.rgb),
							shininess);

	radiance += brdf * irradiance;

  outColor.rgb = lin2rgb(radiance);
  outColor.a = 1.0;
}