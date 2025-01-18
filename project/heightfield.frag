#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;


in vec2 texCoord;
layout(location = 0) out vec4 fragmentColor;

// This simple fragment shader is meant to be used for debug purposes
// When the geometry is ok, we will migrate to use shading.frag instead.
uniform sampler2D heightMap;  // Use as a test if no diffuse texture
uniform sampler2D diffuseMap; // Optional diffuse texture

void main()
{
    // Sample color from the diffuse map (fallback to heightmap for debugging)
    vec3 color = texture(diffuseMap, texCoord).rgb;

	fragmentColor = vec4(texCoord.xy, 0.0, 1.0);
	//fragmentColor = vec4(1, 1, 1, 1);
}
