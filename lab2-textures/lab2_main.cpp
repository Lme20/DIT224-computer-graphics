
#include <GL/glew.h>

// STB_IMAGE for loading images of many filetypes
#include <stb_image.h>

#include <cstdlib>

#include <labhelper.h>

#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////

// The window we'll be rendering to
SDL_Window* g_window = nullptr;

int mag = 1;
int mini = 5;
float anisotropy = 16.0f;
float camera_pan = 0.f;
bool showUI = false;


///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////

// The shaderProgram holds the vertexShader and fragmentShader
GLuint shaderProgram;


///////////////////////////////////////////////////////////////////////////////
// Scene objects
///////////////////////////////////////////////////////////////////////////////

// The vertexArrayObject here will hold the pointers to
// the vertex data (in positionBuffer) and color data per vertex (in colorBuffer)
GLuint positionBuffer, colorBuffer, indexBuffer, vertexArrayObject, texcoordBuffer;
GLuint texture; // set textures as scene objects

GLuint vertBuffer2, indexBuffer2, texcoordBuffer2, vertexArrayObject2;
GLuint texture2;

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	///////////////////////////////////////////////////////////////////////////
	// Create the vertex array object
	///////////////////////////////////////////////////////////////////////////
	// Create a handle for the vertex array object
	glGenVertexArrays(1, &vertexArrayObject);
	// Set it as current, i.e., related calls will affect this object
	glBindVertexArray(vertexArrayObject);

	///////////////////////////////////////////////////////////////////////////
	// Create the positions buffer object
	///////////////////////////////////////////////////////////////////////////
	const float positions[] = {
		// X      Y       Z
		-10.0f, 0.0f, -10.0f,  // v0
		-10.0f, 0.0f, -330.0f, // v1
		10.0f,  0.0f, -330.0f, // v2
		10.0f,  0.0f, -10.0f   // v3
	};
	// Create a handle for the vertex position buffer
	glGenBuffers(1, &positionBuffer);
	// Set the newly created buffer as the current one
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	// Send the vetex position data to the current buffer
	glBufferData(GL_ARRAY_BUFFER, labhelper::array_length(positions) * sizeof(float), positions,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	// Enable the attribute
	glEnableVertexAttribArray(0);

	///////////////////////////////////////////////////////////////////////////
	// Task 1 : Create the texture coordinates.
	//			Create the texture coordinates' buffer object.
	//			Set up the attrib pointer.
	//			Enable the vertex attrib array.
	///////////////////////////////////////////////////////////////////////////

	// texture coordinates for each vertex of the quad
	float texcoords[] = {
		0.0f, 0.0f,    // (u,v) for v0
		0.0f, 15.0f,   // (u,v) for v1
		1.0f, 15.0f,   // (u,v) for v2
		1.0f, 0.0f     // (u,v) for v3
	};

	// generate buffer object for the texture coordinates
	glGenBuffers(1, &texcoordBuffer); // generate unique buffer ID for textcoordBuffer
	glBindBuffer(GL_ARRAY_BUFFER, texcoordBuffer); // Bind textcoordBuffer as current array buffer
	// Send texture coordinates data to the buffer
	glBufferData(GL_ARRAY_BUFFER, labhelper::array_length(texcoords) * sizeof(float), texcoords, GL_STATIC_DRAW);

	// set up the attrib pointer, parameter (2) is attribute location index
	// second parameter (2) specifies the number of components per vertex attribute (u,v)
	// third parameter GL_FLOAT specifies data type of each component
	glVertexAttribPointer(2, 2, GL_FLOAT, false/*normalized*/, 0/*stride*/, 0/*offset*/);

	// Enable the vertex attrib array
	glEnableVertexAttribArray(2);



	///////////////////////////////////////////////////////////////////////////
	// Create the element array buffer object
	///////////////////////////////////////////////////////////////////////////
	const int indices[] = {
		0, 1, 3, // Triangle 1
		1, 2, 3  // Triangle 2
	};
	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, labhelper::array_length(indices) * sizeof(float), indices,
	             GL_STATIC_DRAW);


	// The loadShaderProgram and linkShaderProgam functions are defined in glutil.cpp and
	// do exactly what we did in lab1 but are hidden for convenience
	shaderProgram = labhelper::loadShaderProgram("../lab2-textures/simple.vert",
	                                             "../lab2-textures/simple.frag");

	//**********************************************
	///////////////////////////////////////////////////////////////////////////
	// Create the explosion quad
	///////////////////////////////////////////////////////////////////////////
	const float verts2[] = {
		2.0f,  0.0f,  -20.0f, // v0
		2.0f,  10.0f, -20.0f, // v1
		12.0f, 10.0f, -20.0f, // v2
		12.0f, 0.0f,  -20.0f  // v3
	};
	float texcoords2[] = {
		0.0f, 0.0f, // (u,v) for v0
		0.0f, 1.0f, // (u,v) for v1
		1.0f, 1.0f, // (u,v) for v2
		1.0f, 0.0f  // (u,v) for v3
	};
	const int indices2[] = {
		0, 1, 3, // Triangle 1
		1, 2, 3  // Triangle 2
	};
	// Create the buffer objects
	glGenBuffers(1, &vertBuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, vertBuffer2);
	glBufferData(GL_ARRAY_BUFFER, labhelper::array_length(verts2) * sizeof(float), verts2, GL_STATIC_DRAW);

	glGenBuffers(1, &texcoordBuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, texcoordBuffer2);
	glBufferData(GL_ARRAY_BUFFER, labhelper::array_length(texcoords2) * sizeof(float), texcoords2,
		GL_STATIC_DRAW);

	glGenBuffers(1, &indexBuffer2);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer2);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, labhelper::array_length(indices2) * sizeof(float), indices2,
		GL_STATIC_DRAW);

	glGenVertexArrays(1, &vertexArrayObject2);
	glBindVertexArray(vertexArrayObject2);
	glBindBuffer(GL_ARRAY_BUFFER, vertBuffer2);
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glBindBuffer(GL_ARRAY_BUFFER, texcoordBuffer2);
	glVertexAttribPointer(2, 2, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer2);
	glEnableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	//************************************
	//			Load Texture
	//************************************
	// Task 2

	// Task 2.1
	int w, h, comp;
	unsigned char* image = stbi_load("../scenes/textures/asphalt.jpg", &w, &h, &comp, STBI_rgb_alpha);

	// generate and initialize OpenGL texture
	glGenTextures(1, &texture);
	  
	glBindTexture(GL_TEXTURE_2D, texture); // bind the texture using unique ID
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image); // allocate memory for texture
	stbi_image_free(image); // free up memory used by image data, can also be 	stbi_image_free(image);

	// using coordinate system 0,0 in lower corner and 1,1 in upper right corner
	// specify OpenGL to clam texture coordinates to this specific range
	// Indicates that the active texture should be repeated,
	// instead of for instance clamped, for texture coordinates > 1 or <-1.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// set up texture filtering, use mipmapping
	glGenerateMipmap(GL_TEXTURE_2D);
	// Sets the type of filtering to be used on magnifying and
	// minifying the active texture. These are the nicest available options.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// Enable an extension for better anisotropic filtering
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);

	//Explosion texture
	glGenTextures(1, &texture2);
	// generate and initialize OpenGL texture
	glBindTexture(GL_TEXTURE_2D, texture2); // bind the texture using unique ID
	unsigned char* image2 = stbi_load("../scenes/textures/explosion.png", &w, &h, &comp, STBI_rgb_alpha);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2); // allocate memory for texture
	stbi_image_free(image2);

	//Indicates that the active texture should be repeated,
	//instead of for instance clamped, for texture coordinates > 1 or <-1.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
	// Sets the type of mipmap interpolation to be used on magnifying and
	// minifying the active texture. These are the nicest available options.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	// The viewport determines how many pixels we are rasterizing to
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	// Set viewport
	glViewport(0, 0, w, h);

	// Set clear color
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	// Clears the color buffer and the z-buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// We disable backface culling for this tutorial, otherwise care must be taken with the winding order
	// of the vertices. It is however a lot faster to enable culling when drawing large scenes.
	glDisable(GL_CULL_FACE);
	// glEnable(GL_CULL_FACE); TASK 8
	
	// Disable depth testing
	glDisable(GL_DEPTH_TEST);
	// Set the shader program to use for this draw call
	glUseProgram(shaderProgram);

	// Set up a projection matrix
	float fovy = radians(45.0f);
	float aspectRatio = float(w) / float(h);
	float nearPlane = 0.01f;
	float farPlane = 400.0f;
	mat4 projectionMatrix = perspective(fovy, aspectRatio, nearPlane, farPlane);
	// Send it to the vertex shader
	int loc = glGetUniformLocation(shaderProgram, "projectionMatrix");
	glUniformMatrix4fv(loc, 1, false, &projectionMatrix[0].x);

	loc = glGetUniformLocation(shaderProgram, "cameraPosition");
	glUniform3f(loc, camera_pan, 10, 0);

	// ----- Draw the first object with the first texture -----
	// Task 3.1
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);


	// draw the road
	glBindVertexArray(vertexArrayObject);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// ----- Draw the second object with the second texture -----
	// Use new texture in draw call
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, texture2);
	glActiveTexture(GL_TEXTURE1);


	// draw explosion
	glBindVertexArray(vertexArrayObject2);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// disable blending
	glDisable(GL_BLEND);


	glUseProgram(0); // "unsets" the current shader program. Not really necessary.
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::PushID("mag");
	ImGui::Text("Magnification");
	ImGui::RadioButton("GL_NEAREST", &mag, 0);
	ImGui::RadioButton("GL_LINEAR", &mag, 1);
	ImGui::PopID();

	ImGui::PushID("mini");
	ImGui::Text("Minification");
	ImGui::RadioButton("GL_NEAREST", &mini, 0);
	ImGui::RadioButton("GL_LINEAR", &mini, 1);
	ImGui::RadioButton("GL_NEAREST_MIPMAP_NEAREST", &mini, 2);
	ImGui::RadioButton("GL_NEAREST_MIPMAP_LINEAR", &mini, 3);
	ImGui::RadioButton("GL_LINEAR_MIPMAP_NEAREST", &mini, 4);
	ImGui::RadioButton("GL_LINEAR_MIPMAP_LINEAR", &mini, 5);
	ImGui::PopID();

	ImGui::SliderFloat("Anisotropic filtering", &anisotropy, 1.0, 16.0, "Number of samples: %.0f");
	ImGui::Dummy({ 0, 20 });
	ImGui::SliderFloat("Camera Panning", &camera_pan, -1.0, 1.0);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);


	// Magnification.
	switch (mag)
	{
	case 0:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	case 1:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		break;
	}

	// Minification.
	switch (mini)
	{
	case 0:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		break;
	case 1:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		break;
	case 2:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		break;
	case 3:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		break;
	case 4:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		break;
	case 5:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		break;
	}

	// Anisotropic filtering.
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

	// Reset active texture to default (optional, for cleanliness)
	glActiveTexture(GL_TEXTURE0);
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Lab 2");

	initialize();

	// render-loop
	bool stopRendering = false;
	while(!stopRendering)
	{
		// Inform imgui of new frame
		ImGui_ImplSdlGL3_NewFrame(g_window);

		// check events (keyboard among other)
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			// Allow ImGui to capture events.
			ImGui_ImplSdlGL3_ProcessEvent(&event);

			if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
			{
				stopRendering = true;
			}
			else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
			{
				showUI = !showUI;
			}
			else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_PRINTSCREEN)
			{
				labhelper::saveScreenshot();
			}
		}

		// render to window
		display();

		// Render overlay GUI.
		// if(showUI)
		{
			gui();
		}

		// Render the GUI.
		ImGui::Render();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
