#include "heightfield.h"

#include <iostream>
#include <stdint.h>
#include <vector>
#include <glm/glm.hpp>
#include <stb_image.h>

using namespace glm;
using std::string;

HeightField::HeightField(void)
	: m_meshResolution(0)
	, m_vao(UINT32_MAX)
	, m_positionBuffer(UINT32_MAX)
	, m_uvBuffer(UINT32_MAX)
	, m_indexBuffer(UINT32_MAX)
	, m_numIndices(0)
	, m_texid_hf(UINT32_MAX)
	, m_texid_diffuse(UINT32_MAX)
	, m_heightFieldPath("")
	, m_diffuseTexturePath("")
{
}

void HeightField::loadHeightField(const std::string& heigtFieldPath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(heigtFieldPath.c_str(), &width, &height, &components, 1); 
	if (data == nullptr)
	{
		std::cout << "Failed to load image: " << heigtFieldPath << ".\n";
		return;
	}

	if (m_texid_hf == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_hf);
	}
	glBindTexture(GL_TEXTURE_2D, m_texid_hf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, data); // just one component (float)
	glGenerateMipmap(GL_TEXTURE_2D); // generate mipmaps

	m_heightFieldPath = heigtFieldPath;
	std::cout << "Successfully loaded heigh field texture: " << heigtFieldPath << ".\n";
}

void HeightField::loadDiffuseTexture(const std::string& diffusePath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	uint8_t* data = stbi_load(diffusePath.c_str(), &width, &height, &components, 3);
	if (data == nullptr)
	{
		std::cout << "Failed to load image: " << diffusePath << ".\n";
		return;
	}

	if (m_texid_diffuse == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_diffuse);
	}

	glBindTexture(GL_TEXTURE_2D, m_texid_diffuse);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); // plain RGB
	glGenerateMipmap(GL_TEXTURE_2D);

	std::cout << "Successfully loaded diffuse texture: " << diffusePath << ".\n";
}


void HeightField::generateMesh(int tesselation)
{
	// Step 1: generate a mesh in range -1 to 1 in x and z
	// (y is 0 but will be altered in height field vertex shader)
	// number of triangles determined by tesselation 
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<GLuint> indices;

	float step = 2.0f / tesselation; // step size to cover [-1, 1]

	for (int z = 0; z <= tesselation; ++z) {
		for (int x = 0; x <= tesselation; ++x) {

			/// MESH DEFINITION
			// Position: Range [-1, 1] for x and z
			float px = -1.0f + x * step;
			float pz = -1.0f + z * step;
			positions.emplace_back(glm::vec3(px, 0.0f, pz));


			// Texture coordinates: Range [0, 1] for u and v
			float u = float(x) / tesselation;
			float v = float(z) / tesselation;
			uvs.emplace_back(glm::vec2(u, v));

			// indices for triangle mesh
			if (x < tesselation && z < tesselation) {
				GLuint topLeft = z * (tesselation + 1) + x;
				GLuint topRight = topLeft + 1;
				GLuint bottomLeft = topLeft + (tesselation + 1);
				GLuint bottomRight = bottomLeft + 1;

				indices.insert(indices.end(), { topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight });
			}
		}
	}

	// Step 2: Send data to OpenGL buffers
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	// Positions
	glGenBuffers(1, &m_positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);

	// Texture coordinates
	glGenBuffers(1, &m_uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_uvBuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(2);

	// Indices
	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

	m_numIndices = indices.size();

	// Unbind VAO
	glBindVertexArray(0);

}

void HeightField::submitTriangles(void)
{

	// Step 3: Bind VAO and draw
	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
}

// Get height at a given point - USED FOR TREE PLACEMENT
float HeightField::sampleHeightAt(float u, float v) const
{
	// u,v should be within [0, 1]
	u = glm::clamp(u, 0.0f, 1.0f);
	v = glm::clamp(v, 0.0f, 1.0f);

	// Bind the height map texture
	glBindTexture(GL_TEXTURE_2D, m_texid_hf);

	// Get the texture dimensions
	int width, height;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	// Calculate pixel indices
	int x = static_cast<int>(u * (width - 1));
	int y = static_cast<int>(v * (height - 1));

	// Read pixel data
	std::vector<float> pixelData(width * height);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, pixelData.data());

	// Sample the height value
	float rawHeight = pixelData[y * width + x];

	// Apply height scaling (consistent with shader)
	const float heightScale = 10.0f;  // Match the shader's `heightScale` uniform for terrain
	return rawHeight * heightScale;
}






