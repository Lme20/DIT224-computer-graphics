#version 420
///////////////////////////////////////////////////////////////////////////////
// Input vertex attributes
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 position;      // Vertex position
layout(location = 2) in vec2 texCoordIn;    // Texture coordinates

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 modelViewProjectionMatrix;     // MVP matrix
uniform mat4 modelViewMatrix;               // Model-View matrix
uniform mat4 normalMatrix;                  // Normal transformation matrix
uniform sampler2D heightMap;                // Height map texture
uniform float heightScale;                  // Scaling factor for height

///////////////////////////////////////////////////////////////////////////////
// Output to fragment shader
///////////////////////////////////////////////////////////////////////////////
out vec2 texCoord;                          // Pass texture coordinates
out vec3 viewSpaceNormal;                   // Pass normal in view space
out vec3 viewSpacePosition;                 // Pass position in view space

///////////////////////////////////////////////////////////////////////////////
// Main Shader Code
///////////////////////////////////////////////////////////////////////////////
void main() {
    // Step 1: Displace the position in the y-axis based on the height map
    float height = texture(heightMap, texCoordIn).r * heightScale;
    vec3 displacedPosition = position + vec3(0.0, height, 0.0);

    // Step 2: Compute gradients in u and v directions
    float du = 0.001; // Small offset for gradient computation
    float dv = 0.001;

    // Sample neighboring heights
    float heightL = texture(heightMap, texCoordIn + vec2(-du, 0)).r * heightScale;
    float heightR = texture(heightMap, texCoordIn + vec2(du, 0)).r * heightScale;
    float heightD = texture(heightMap, texCoordIn + vec2(0, -dv)).r * heightScale;
    float heightU = texture(heightMap, texCoordIn + vec2(0, dv)).r * heightScale;

    // Tangent vectors
    vec3 tangentU = normalize(vec3(2.0 * du, heightR - heightL, 0.0));
    vec3 tangentV = normalize(vec3(0.0, heightU - heightD, 2.0 * dv));

    // Compute normal using the cross product
    vec3 normal = normalize(cross(tangentV, tangentU));

    // Step 3: Transform position and normal to view space
    viewSpacePosition = (modelViewMatrix * vec4(displacedPosition, 1.0)).xyz;
    viewSpaceNormal = (normalMatrix * vec4(normal, 0.0)).xyz;

    // Step 4: Set final position and pass texture coordinates
    gl_Position = modelViewProjectionMatrix * vec4(displacedPosition, 1.0);
    texCoord = texCoordIn;
}
