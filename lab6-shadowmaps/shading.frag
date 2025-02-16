#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform vec3 material_emission;

uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
uniform int has_emission_texture;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

// task 6
uniform int useSpotLight;
uniform int useSoftFalloff;
uniform vec3 viewSpaceLightDir;
uniform float spotInnerAngle;
uniform float spotOuterAngle;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
in vec4 shadowMapCoord; // TASK 7

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

// TASK 2
uniform mat4 lightMatrix;

// #if SOLUTION_USE_BUILTIN_SHADOW_TEST
layout(binding = 10) uniform sampler2DShadow shadowMapTex; // task 7
// #else  // USE_BUILTIN_SHADOW_TEST
// layout(binding = 10) uniform sampler2D shadowMapTex; // Shadow map lookup
// #endif // ~ USE_BUILTIN_SHADOW_TEST


// Taken from Tutorial 4
vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
	vec3 direct_illum = base_color;
	///////////////////////////////////////////////////////////////////////////
	//            Calculate the radiance Li from the light, and the direction
	//            to the light. If the light is backfacing the triangle,
	//            return vec3(0);
	///////////////////////////////////////////////////////////////////////////
	
	// 1. we first calculate the distance from the light to the fragment
	const float distance_to_light = length(viewSpaceLightPosition - viewSpacePosition);

	// 2. we then calculate the falloff factor based on the inverse square law
	const float falloff_factor = 1.0 / (distance_to_light * distance_to_light);

	// 3. calculate the radiance Li using the light intensity, color, and falloff
	vec3 Li = point_light_intensity_multiplier * point_light_color * falloff_factor;

	// 4. and finally calculate the normalized direction to the light source
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);

	// 5. Check if the light is backfacing the surface by evaluating dot product
	if (dot(wi, n) <= 0.0)
	{
		return vec3(0.0); // early exit if light is backfacing the surface
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Task 1.3 - Calculate the diffuse term and return that as the result
	///////////////////////////////////////////////////////////////////////////
	float dot_n_wi = dot(n, wi); // declare absolute value of n � wi

	// Following the given formula, using abs_dot_n_wi and Li declared in task 1.2
	vec3 diffuse_term = (1.0 / PI) * base_color * dot_n_wi * Li;

	direct_illum = diffuse_term; // task 1.3

	///////////////////////////////////////////////////////////////////////////
	//          Calculate the Torrance Sparrow BRDF and return the light
	//          reflected from that instead
	///////////////////////////////////////////////////////////////////////////

	// 1. calculate normalized half-vector
	vec3 wh = normalize(wi + wo);

	// 2. calculate the dot products needed for the BRDF
	float ndotwh = max(0.0001, dot(n, wh)); // dot product between normal and halfvector
	float ndotwo = max(0.0001, dot(n, wo)); // dot product between normal and view direction
	float wodotwh = max(0.0001, dot(wo, wh)); // dot product between view direction and halfvector

	// 3. Microfacet distribution function (D), using Blinn-Phong's
	float D = ((material_shininess + 2) / (2.0 * PI)) * pow(ndotwh, material_shininess);

	// 4. Geometry function G - calculating the shadowing/masking term
	float G = min(1.0, min(2.0 * ndotwh * ndotwo / wodotwh, 2.0 * ndotwh * dot_n_wi / wodotwh));

	// 2. Fresnel term F with Schilk's approximation
	float F = material_fresnel + (1.0 - material_fresnel) * pow(1.0 - wodotwh, 5.0);

	// 5. BRDF - combining terms above to calculate BRDF
	// made a denominator variable clamping a small positive value to avoid dividing by zero
	// adding the denominator fixed the pink pixels issue
	float denominator = max(4.0 * ndotwo * dot_n_wi, 0.0001);

	// float denominator = 4.0 * clamp(ndotwo * dot_n_wi, 0.0001, 1.0);
	float brdf = D * F * G / denominator;

	// 6. Reflected light - We multiply the BRDF by incoming radiance and dot product of n and wi
	// vec3 reflectedLight = brdf * dot(n, wi) * Li;


	///////////////////////////////////////////////////////////////////////////
	//         Make your shader respect the parameters of our material model.
	///////////////////////////////////////////////////////////////////////////

	// 1. Combining specular reflection and diffuse reflection for dielectrics.
	vec3 dielectric_term = brdf * dot_n_wi * Li + (1.0 - F) * diffuse_term;

	// 2. Reflect light should take the color of the material for metals
	vec3 metal_term = brdf * base_color * dot_n_wi * Li;

	// 3. Blend the 2 light reflections above (dielectric, metal terms) with materiu
	vec3 microfacet_term = material_metalness * metal_term + (1.0 - material_metalness) * dielectric_term;
	direct_illum = microfacet_term;


	// return brdf * dot(n, wi) * Li; // task 2
	// return diffuse_term; // task 1.3
	return direct_illum;

	// debugging
	// return vec3(D);
	// return vec3(G);
	// return vec3(F);
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n, vec3 base_color)
{
	vec3 indirect_illum = vec3(0.f);
	///////////////////////////////////////////////////////////////////////////
	//          Lookup the irradiance from the irradiance map and calculate
	//          the diffuse reflection
	///////////////////////////////////////////////////////////////////////////
	// Transform normal from viewspace to worldspace with view inverse matrix
	vec3 world_normal = vec3(viewInverse * vec4(n, 0.0));

	// Calculate the spherical coordinates of the world-space normal
	float theta = acos(max(-1.0f, min(1.0f, world_normal.y))); // clamp, theta: polar angle
	float phi = atan(world_normal.z, world_normal.x); // azimuthal angle

	// Ensure phi is positive 
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Normalize spherical coordinates to map irradience texture
	vec2 lookup = vec2(phi / (2.0 * PI), 1 - theta / PI);

	// fetch irradience from precomputed irradience map with lookup coordinates
	vec3 Li = environment_multiplier * texture(irradianceMap, lookup).rgb;

	// calculate diffuse reflection term 
	vec3 diffuse_term = base_color * (1.0 / PI) * Li;

	// set the resulting indirect illumination to the computed diffuse term
	indirect_illum = diffuse_term;

	///////////////////////////////////////////////////////////////////////////
	//          Look up in the reflection map from the perfect specular
	//          direction and calculate the dielectric and metal terms.
	///////////////////////////////////////////////////////////////////////////

	// calculate relfection vector wi in view space
	vec3 wi = normalize(reflect(-wo, n));

	// Transform reflection vector from view space to world space
	vec3 wr = normalize(vec3(viewInverse * vec4(wi, 0.0)));

	// Calculate the spherical coordinates of the world-space reflection vector
	// we reuse theta and phi from task 5
	theta = acos(max(-1.0f, min(1.0f, wr.y))); // calculate Polar angle
	phi = atan(wr.z, wr.x); // calculate Azimuthal angle
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI; // shift phi to be positive
	}

	// Map spherical coordinates to normalized texture coordinates for lookup
	lookup = vec2(phi / (2.0 * PI), 1 - theta / PI);

	// Fetch preconvolved radiance from relfectiom map using mip level based on roughness
	float roughness = sqrt(sqrt(2.0 / (material_shininess + 2.0))); // Roughness from shininess
	Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).rgb;

	// calculate halfvector for reflection
	vec3 wh = normalize(wi + wo); // Half-vector
	float wodotwh = max(0.0, dot(wo, wh)); // Dot product between wo and wh

	// calculate fresnel term with Schilk's approximation
	float F = material_fresnel + (1.0 - material_fresnel) * pow(1.0 - wodotwh, 5.0);

	// calculate dielectric and metal terms
	vec3 dielectric_term = F * Li + (1.0 - F) * diffuse_term; // Dielectric
	vec3 metal_term = F * base_color * Li; // Metal

	// Blend the two terms with the material metalness
	vec3 microfacet_term = material_metalness * metal_term + (1.0 - material_metalness) * dielectric_term;

	// set indirect illumination to computed microfacet term
	indirect_illum = microfacet_term;


	return indirect_illum;
}

void main()
{
	float visibility = 1.0;
	float attenuation = 1.0;

	// TASK 2 - Calculate shadow map coordinates
	// vec4 shadowMapCoord = lightMatrix * vec4(viewSpacePosition, 1.0); moved to shading.vert

	// TASK 2 - Calculate shadow map visibility
	// #if SOLUTION_USE_BUILTIN_SHADOW_TEST // SOLUTION_CODE >= 8
		visibility = textureProj(shadowMapTex, shadowMapCoord);
	// #else
		// float depth = texture(shadowMapTex, shadowMapCoord.xy / shadowMapCoord.w).r;
		// visibility = (depth >= (shadowMapCoord.z / shadowMapCoord.w)) ? 1.0 : 0.0; // compute visibility
	// #endif // ~ USE_BUILTIN_SHADOW_TEST

	// TASK 6 - Calculate spotlight attenuation
	attenuation = 1.0;
	if(useSpotLight == 1) // if spotlight is enabled
	{
		// Calculate the angle between the fragment and the light direction
		vec3 posToLight = normalize(viewSpaceLightPosition - viewSpacePosition);
		float cosAngle = dot(posToLight, -viewSpaceLightDir);

		// Calculate the attenuation based on the angle
		if(useSoftFalloff == 0)
		{
			// Spotlight with hard border:
			attenuation = (cosAngle > spotOuterAngle) ? 1.0 : 0.0;
		}
		// if useSoftFalloff is enabled, apply smooth borders
		else 
		{
			// Spotlight with soft border:
			attenuation = smoothstep(spotOuterAngle, spotInnerAngle, cosAngle);
		}

		// Multiply the visibility by the attenuation
		visibility *= attenuation;
	}

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	vec3 base_color = material_color;
	if(has_color_texture == 1)
	{
		base_color = texture(colorMap, texCoord).rgb;
	}

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n, base_color);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n, base_color);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).rgb;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	fragmentColor = vec4(shading, 1.0);
	return;
}
