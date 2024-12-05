#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// Task 1: Receive the texture coordinates
in vec2 texCoord; //incoming texture coordinates from vertex shader

// Task 3.4: Receive the texture as a uniform
layout(binding = 0) uniform sampler2D colortexture;
layout(binding = 1) uniform sampler2D explosionTexture;


layout(location = 0) out vec4 fragmentColor;

void main()
{
	// Task 1: Use the texture coordinates for the x,y of the color output
	// Task 3.5: Sample the texture with the texture coordinates and use that for the color
	// fragmentColor = vec4(texCoord.x, texCoord.y, 0.0, 0.0);
    if (gl_FrontFacing) {
        fragmentColor = texture(explosionTexture, texCoord);  // Use explosion texture for front face
    } else {
        fragmentColor = texture(colortexture, texCoord);      // Use road texture for back face
    }
}
