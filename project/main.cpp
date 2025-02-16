

#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <iostream>

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"


#include "heightfield.h"


using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

HeightField terrain; // Heightfield object

labhelper::Model* treeModel = nullptr;  // declare tree model globally
std::vector<glm::vec3> treePositions;   // store tree positions

bool useWireframe = false; // toggle wireframe mode

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint terrainShaderProgram; // Shader used to draw the terrain


///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;



///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

float shipSpeed = 50;


void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag",
	                                             is_reload);
	if(shader != 0)
	{
		simpleShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/fullscreenQuad.vert", "../project/background.frag",
	                                      is_reload);
	if(shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}

	// Load terrain shader
	shader = labhelper::loadShaderProgram("../project/heightfield.vert", "../project/shading.frag", is_reload);
	if (shader != 0)
	{
		terrainShaderProgram = shader;
	}
	else
	{
		std::cerr << "Failed to load terrain shader." << std::endl;
	}
}

// Function to generate tree positions randomly at a specific height range
void generateTreePositions()
{
	const int maxTrees = 500;  // max number of trees
	const int numTreeAttempts = 1000;  // number of random placement attempts
	const float minHeight = 0.1f;  // Minimum height for tree placement
	const float maxHeight = 1.0f;  // Maximum height for tree placement

	// Fetch the terrain scaling from terrainModelMatrix
	const float terrainScaleX = 100.0f; // Matches terrainModelMatrix scaling
	const float terrainScaleZ = 100.0f;

	int generatedTreeCount = 0;

	// Log terrain bounds for debugging
	std::cout << "Terrain bounds: X = [-" << terrainScaleX / 2 << ", " << terrainScaleX / 2
		<< "], Z = [-" << terrainScaleZ / 2 << ", " << terrainScaleZ / 2 << "]" << std::endl;

	// Grid-based sampling to ensure coverage
	const int gridResolution = 50; 
	const float gridStep = 1.0f / gridResolution;

	for (int gx = 0; gx < gridResolution; ++gx)
	{
		for (int gz = 0; gz < gridResolution; ++gz)
		{
			// Uniform grid sampling
			float u = gx * gridStep;
			float v = gz * gridStep;

			// add randomness within each grid cell
			u += (static_cast<float>(rand()) / RAND_MAX) * gridStep * 0.5f;
			v += (static_cast<float>(rand()) / RAND_MAX) * gridStep * 0.5f;

			// we sample height at (u, v)
			float height = terrain.sampleHeightAt(u, v);
			std::cout << "Sampled height at (" << u << ", " << v << "): " << height << '\n'; // DEBUG

			// skip if height is out of range
			if (height < minHeight || height > maxHeight)
			{
				continue;
			}

			// Map u, v to world space coordinates
			float worldX = (u - 0.5f) * terrainScaleX;  // Match terrain scaling
			float worldZ = (v - 0.5f) * terrainScaleZ;
			float worldY = height;  // Use sampled height directly

			// Add the tree position to the list where valid
			treePositions.emplace_back(glm::vec3(worldX, worldY, worldZ));
			++generatedTreeCount;

			// Stop if we reach the maximum number of trees
			if (generatedTreeCount >= maxTrees)
			{
				std::cout << "Reached maximum tree limit: " << maxTrees << std::endl;
				return;
			}
		}
	}

	std::cout << "Generated " << treePositions.size() << " trees." << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/space-ship.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);
	 
	treeModel = labhelper::loadModelFromOBJ("../scenes/tree.obj"); // Load tree model


	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for(int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");
	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);

	///////////////////////////////////////////////////////////////////////
	// Generate the height field terrain
	// using an indexed array of triangles
	///////////////////////////////////////////////////////////////////////
	terrain.generateMesh(512); // tesselation value
	terrain.loadHeightField("../scenes/nlsFinland/L3123F.png"); // Path to your height map
	terrain.loadDiffuseTexture("../scenes/nlsFinland/L3123F_downscaled.jpg"); // Optional diffuse texture


	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling

	// glEnable(GL_PRIMITIVE_RESTART);
	// glPrimitiveRestartIndex(UINT32_MAX);
	
	///////////////////////////////////////////////////////////////////////
	// Generate tree positions
	///////////////////////////////////////////////////////////////////////

	generateTreePositions();

}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(simpleShaderProgram);
	labhelper::setUniformSlow(simpleShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(simpleShaderProgram, "material_color", vec3(1, 1, 1));
	labhelper::debugDrawSphere();
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);

	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	/*
	// landing pad
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * fighterModelMatrix)));

	labhelper::render(fighterModel);
	*/
}

///////////////////////////////////////////////////////////////////////////
// Function to render trees
///////////////////////////////////////////////////////////////////////////
void renderTrees(const glm::mat4& projMatrix, const glm::mat4& viewMatrix)
{
	glUseProgram(shaderProgram); // default scene shader for trees

	//lighting uniforms
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(shaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(shaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(shaderProgram, "point_light_intensity_multiplier", point_light_intensity_multiplier);

	// environment map uniforms
	labhelper::setUniformSlow(shaderProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(shaderProgram, "viewInverse", inverse(viewMatrix));

	// Loop over all tree positions to render them
	const float treeScale = 0.2f; // TREE SIZE: Scale factor for the trees
	for (const glm::vec3& position : treePositions)
	{
		// Computed model matrix for each tree
		glm::mat4 modelMatrix =
			glm::translate(position) * glm::scale(glm::vec3(treeScale)); // apply the scaling
		glm::mat4 mvpMatrix = projMatrix * viewMatrix * modelMatrix;

		// model uniforms
		labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", mvpMatrix);
		labhelper::setUniformSlow(shaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);
		labhelper::setUniformSlow(shaderProgram, "normalMatrix", inverse(transpose(viewMatrix * modelMatrix)));

		// render the tree
		labhelper::render(treeModel);
	}
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}


	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	glActiveTexture(GL_TEXTURE0);

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawBackground(viewMatrix, projMatrix);
	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));

	///////////////////////////////////////////////////////////////////////////
	// Render the height field
	///////////////////////////////////////////////////////////////////////////
	
	// Apply polygon mode based on GUI toggle
	// conditionally enable wireframe mode
	if (useWireframe)
	{

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // enable wireframe mode
		glEnable(GL_DEPTH_TEST);                 // disable depth test for wireframe
		// glDisable(GL_BLEND);                      // blending off
	}

	glUseProgram(terrainShaderProgram); // Use terrain shader for rendering

	// terrain uniforms
	mat4 terrainModelMatrix = scale(mat4(1.0f), vec3(100.0f, 1.0f, 100.0f)); // Scale terrain in x and z directions
	mat4 terrainModelViewProjectionMatrix = projMatrix * viewMatrix * terrainModelMatrix; // compute model view projection matrix

	// transform matrices
	labhelper::setUniformSlow(terrainShaderProgram, "modelViewProjectionMatrix", terrainModelViewProjectionMatrix); // pass MVP matrix to shader
	labhelper::setUniformSlow(terrainShaderProgram, "modelViewMatrix", viewMatrix * terrainModelMatrix); // pass MV matrix for position transformation
	labhelper::setUniformSlow(terrainShaderProgram, "normalMatrix", inverse(transpose(viewMatrix * terrainModelMatrix))); // pass normal matrix for lighting calculations
	labhelper::setUniformSlow(terrainShaderProgram, "heightMap", 0); // Bind height map to texture unit 0 - relevant to heightfield.vert
	labhelper::setUniformSlow(terrainShaderProgram, "heightScale", 10.0f); //set scale for height (y-axis) displacement - relevant to heightfield.vert

	// Light uniforms
	labhelper::setUniformSlow(terrainShaderProgram, "viewSpaceLightPosition", vec3(viewMatrix * vec4(lightPosition, 1.0))); // pass light position transformed in view space
	labhelper::setUniformSlow(terrainShaderProgram, "point_light_color", point_light_color); // pass light color
	labhelper::setUniformSlow(terrainShaderProgram, "point_light_intensity_multiplier", point_light_intensity_multiplier); // pass light intensity multiplier

	// labhelper::setUniformSlow(terrainShaderProgram, "diffuseMap", 1);  // Diffuse texture unit
	// labhelper::setUniformSlow(terrainShaderProgram, "scaleFactor", 100.0f); // modifies size in x and z directions.

	// Diffuse texture
	labhelper::setUniformSlow(terrainShaderProgram, "has_color_texture", 1); // Set to 1 if diffuse texture is used
	labhelper::setUniformSlow(terrainShaderProgram, "colorMap", 1); // Bind diffuse texture to texture unit 1 - shading.frag

	// Environment maps
	labhelper::setUniformSlow(terrainShaderProgram, "environment_multiplier", environment_multiplier);

	//Bind height map texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain.m_texid_hf); // Bind height map

	// Bind diffuse texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, terrain.m_texid_diffuse); // Bind diffuse areal photo - texture

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap); // Environment map


	// Submit the terrain mesh
	terrain.submitTriangles();

	// Apply polygon mode based on GUI toggle
	// disable wireframe after rendering the terrain
	if (useWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // reset to fill mode
		// glEnable(GL_DEPTH_TEST);                  // re-enable depth test
		// glEnable(GL_BLEND);                       // Re-enable blending
	}

	// render trees
	renderTrees(projMatrix, viewMatrix);

}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// Allow ImGui to capture events.
	ImGuiIO& io = ImGui::GetIO();

	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		ImGui_ImplSdlGL3_ProcessEvent(&event);

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			showUI = !showUI;
		}
		else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_PRINTSCREEN)
		{
			labhelper::saveScreenshot();
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!showUI || !io.WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging && !io.WantCaptureMouse)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.4f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);

	static bool was_shift_pressed = state[SDL_SCANCODE_LSHIFT];
	if(was_shift_pressed && !state[SDL_SCANCODE_LSHIFT])
	{
		cameraSpeed /= 5;
	}
	if(!was_shift_pressed && state[SDL_SCANCODE_LSHIFT])
	{
		cameraSpeed *= 5;
	}
	was_shift_pressed = state[SDL_SCANCODE_LSHIFT];


	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}
	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// Terrain properties for GUI
	const vec3 terrainScale(100.0f, 1.0f, 100.0f); // match terrainModelMatrix scaling
	const int terrainTessellation = 512; // match  tessellation value used in `generateMesh`
	const float terrainWidth = terrainScale.x * 2.0f; // Width in world units
	const float terrainDepth = terrainScale.z * 2.0f; // depth in world units

	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// tree count
	ImGui::Text("Number of trees: %d", static_cast<int>(treePositions.size()));

	// Terrain details
	ImGui::Separator(); // Adds a horizontal line
	ImGui::Text("Terrain properties:");
	ImGui::Text("Scale: X=%.1f, Y=%.1f, Z=%.1f", terrainScale.x, terrainScale.y, terrainScale.z);
	ImGui::Text("Width: %.1f units", terrainWidth);
	ImGui::Text("Depth: %.1f units", terrainDepth);
	ImGui::Text("Tessellation: %d (triangles per side)", terrainTessellation);

	// toggle wireframe mode
	ImGui::Separator();
	ImGui::Checkbox("Wireframe Mode", &useWireframe);
	// ----------------------------------------------------------
}

int main(int argc, char* argv[])
{
	// TODO: Add option to scale up or down the terrain
	// TODO: Add option to move the light
	// TODO: Add water?
	// TODO: disable default scene objects

	g_window = labhelper::init_window_SDL("OpenGL Project");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// Inform imgui of new frame
		ImGui_ImplSdlGL3_NewFrame(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// render to window
		display();

		// Render overlay GUI.
		if(showUI)
		{
			gui();
		}

		// Render the GUI.
		ImGui::Render();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(treeModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
