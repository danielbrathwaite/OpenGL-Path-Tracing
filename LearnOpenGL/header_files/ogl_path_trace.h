#ifndef OGL_PATH_TRACE_H
#define OGL_PATH_TRACE_H




#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>
#include <shader_c.h>
#include <geometry_loader.h>

#include <sphere.h>
#include <bvh.h>

#include <cmath>
#include <iomanip>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void renderQuad();
void handleMovementInput(GLFWwindow* window, int key, int scancode, int action, int mods);
void updateCameraBuffer();
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void setupBuffers(int &numSpheres, int &numTriangles, int &numMaterials, int &numNodes);

GLuint sphereSSbo;
GLuint triangleSSbo;
GLuint materialSSbo;
GLuint cameraSSbo;
GLuint bvhSSbo;

const float PI = 3.141592f;

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

// texture size
const unsigned int TEXTURE_WIDTH = 1000;
const unsigned int TEXTURE_HEIGHT = 800;

// timing 
float deltaTime = 0.0f;
float lastFrameTime = 0.0f; // time of last frame

// camera and movement
glm::vec4 camera_position = glm::vec4(0.0, -6.0, 1.0, 0.0);
glm::vec4 camera_direction = glm::vec4(0.0, 1.0, 0.0, 0.0);

float moveSpeed = 10.0;
float rotSpeed = 0.1;

bool mF, mB, mL, mR, mU, mD, mC;
double pxpos = 0, pypos = 0; // previous mouse coordinates


int userDefinedDisplayMode = 1;
int displayMode = 1;
int userDefinedAccumulate = 1;
int accumulate = 0;
bool frameMessage = true;



int run()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSwapInterval(0);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// query limitations
	// -----------------
	int max_compute_work_group_count[3];
	int max_compute_work_group_size[3];
	int max_compute_work_group_invocations;

	for (int idx = 0; idx < 3; idx++) {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
	}
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);

	// build and compile shaders
	// -------------------------
	Shader screenQuad("screenQuadVert.c", "screenQuadFrag.c");
	ComputeShader computeShader("computeShader.c");

	screenQuad.use();
	screenQuad.setInt("tex", 0);

	// Create texture for opengl operation
	// -----------------------------------
	unsigned int texture;

	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);


	//Set pixel color to the nearest texture value, no blurring
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	int numTris, numSpheres, numMaterials, numNodes;
	setupBuffers(numTris, numSpheres, numMaterials, numNodes); // initialize buffer data and send to shaders

	//I have no idea what I am doing
	glfwSetKeyCallback(window, handleMovementInput);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, cursorPosCallback);

	// render loop
	// -----------
	int frameCount = 0;
	float lastMessage = (float)glfwGetTime();
	while (!glfwWindowShouldClose(window))
	{
		updateCameraBuffer();

		frameCount++;
		// Set frame time
		float currentTime = (float)glfwGetTime();
		deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;
		if (frameMessage && (currentTime - lastMessage > 1)) {
			lastMessage = currentTime;
			std::cout << "\r" << setw(20) << left << "FPS: " << setw(10) << 1 / deltaTime << "  # Writes : " << frameCount << "   " << std::flush;
		}

		computeShader.use(); 
		computeShader.setFloat("t", currentTime);
		computeShader.setInt("frame", frameCount);
		computeShader.setInt("numSpheres", numSpheres);
		computeShader.setInt("numTriangles", numTris);
		computeShader.setInt("numMaterials", numMaterials);
		computeShader.setInt("numNodes", numNodes);
		computeShader.setInt("accumulate", accumulate);
		computeShader.setInt("displayMode", displayMode);
		glDispatchCompute((unsigned int)TEXTURE_WIDTH / 10, (unsigned int)TEXTURE_HEIGHT / 10, 1);

		// make sure writing to image has finished before read
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// render image to quad
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		screenQuad.use();

		renderQuad();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();

		accumulate = userDefinedAccumulate;
		if (mF || mR || mB || mL || mU || mD || mC) {
			accumulate = 0;
			frameCount = 0;
		}
		mC = false;
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteTextures(1, &texture);
	glDeleteProgram(screenQuad.ID);
	glDeleteProgram(computeShader.ID);

	glfwTerminate();

	return EXIT_SUCCESS;
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void handleMovementInput(GLFWwindow* window, int key, int scancode, int action, int mods) {

	int prevDisplayMode = displayMode;
	if (key == GLFW_KEY_1) displayMode = 1;
	if (key == GLFW_KEY_2) displayMode = 2;
	if (key == GLFW_KEY_3) displayMode = 3;
	if (key == GLFW_KEY_4) displayMode = 4;
	if (prevDisplayMode != displayMode) mC = true;


	if (key == GLFW_KEY_W) {
		if (action == GLFW_PRESS) mF = true;
		if (action == GLFW_RELEASE) mF = false;
	}

	if (key == GLFW_KEY_A) {
		if (action == GLFW_PRESS) mL = true;
		if (action == GLFW_RELEASE) mL = false;
	}

	if (key == GLFW_KEY_S) {
		if (action == GLFW_PRESS) mB = true;
		if (action == GLFW_RELEASE) mB = false;
	}

	if (key == GLFW_KEY_D) {
		if (action == GLFW_PRESS) mR = true;
		if (action == GLFW_RELEASE) mR = false;
	}

	if (key == GLFW_KEY_SPACE) {
		if (action == GLFW_PRESS) mU = true;
		if (action == GLFW_RELEASE) mU = false;
	}

	if (key == GLFW_KEY_LEFT_SHIFT) {
		if (action == GLFW_PRESS) mD = true;
		if (action == GLFW_RELEASE) mD = false;
	}

	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
}

void updateCameraBuffer() {
	glm::vec4 forward = glm::normalize(camera_direction - glm::vec4(0.0, 0.0, camera_direction.z, 0.0));
	glm::vec3 up3 = glm::vec3(0.0, 0.0, 1.0);
	glm::vec4 up = glm::vec4(0.0, 0.0, 1.0, 0.0);
	glm::vec3 forward3 = glm::vec3(forward.x, forward.y, forward.z);
	glm::vec3 right3 = glm::cross(forward3, up3);
	glm::vec4 right = glm::vec4(right3.x, right3.y, right3.z, 0.0);

	if (mF)	camera_position += forward * moveSpeed * deltaTime;

	if (mL)	camera_position -= right * moveSpeed * deltaTime;

	if (mB)	camera_position -= forward * moveSpeed * deltaTime;

	if (mR)	camera_position += right * moveSpeed * deltaTime;

	if (mU)	camera_position += up * moveSpeed * deltaTime;

	if (mD)	camera_position -= up * moveSpeed * deltaTime;


	glm::vec4 camera_buf_data[3] = { camera_position, camera_direction, glm::vec4(0.0) };

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(glm::vec4), camera_buf_data, GL_STATIC_DRAW); //sizeof(data) only works for statically sized C/C++ arrays.
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cameraSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind
}


//this may be scuffed
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	mC = true;

	double rotationAroundZ = glm::radians(pxpos - xpos) * rotSpeed;// *deltaTime;
	double rotationAroundHoriz = glm::radians(pypos - ypos) * rotSpeed;// *deltaTime;

	double lengthFromZPerspective = sqrt(camera_direction.x * camera_direction.x + camera_direction.y * camera_direction.y);
	double currentZAngle = atan2(camera_direction.y, camera_direction.x);

	double newZAngle = currentZAngle + rotationAroundZ;
	double tnewX = lengthFromZPerspective * cos(newZAngle);
	double tnewY = lengthFromZPerspective * sin(newZAngle);



	double length = sqrt(camera_direction.x * camera_direction.x + camera_direction.y * camera_direction.y + camera_direction.z * camera_direction.z);

	double currentHAngle = atan2(camera_direction.z, lengthFromZPerspective);
	double newHAngle = currentHAngle + rotationAroundHoriz;
	if (newHAngle > PI / 2.0 || newHAngle < -PI / 2.0) newHAngle = currentHAngle;

	double newZ = length * sin(newHAngle);
	double newXYLength = length * cos(newHAngle);
	double newX = newXYLength * cos(newZAngle);
	double newY = newXYLength * sin(newZAngle);

	camera_direction = glm::vec4(newX, newY, newZ, 0.0);



	pxpos = xpos;
	pypos = ypos;
}


void setupBuffers(int &numTris, int &numSpheres, int &numMaterials, int &numNodes) {

	

	cout << "Setting up buffers" << endl;
	vector<Triangle> trivect;
	vector<Material> matvect;
	//load_vertex_data("scene_data/driftobj.txt", "scene_data/driftmtl.txt", trivect, matvect);
	load_vertex_data("scene_data/freeobj.txt", "scene_data/freemtl.txt", trivect, matvect);
	//load_vertex_data("scene_data/p2obj.txt", "scene_data/p2mtl.txt", trivect, matvect);
	//load_vertex_data("scene_data/Racerobj.txt", "scene_data/Racermtl.txt", trivect, matvect);

	numTris = trivect.size();

	cout << setw(20) << left << "# of polygons: " << numTris << endl;

	glGenBuffers(1, &triangleSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSbo);
	GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; // the invalidate makes a big difference when re-writing
	glBufferData(GL_SHADER_STORAGE_BUFFER, numTris * sizeof(struct Triangle), NULL, GL_STATIC_DRAW);

	Triangle* tris = (Triangle*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, numTris * sizeof(Triangle), bufMask);

	for (int triind = 0; triind < numTris; triind++) {
		tris[triind] = trivect[triind];
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);



	vector<BVH> heirarchy = buildSAHTree(trivect);
	numNodes = heirarchy.size();

	glGenBuffers(1, &bvhSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, numNodes * sizeof(struct BVH), NULL, GL_STATIC_DRAW);

	BVH* heir = (BVH*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, numNodes * sizeof(BVH), bufMask);

	for (int bvhind = 0; bvhind < numNodes; bvhind++) {
		heir[bvhind] = heirarchy[bvhind];
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);



	Material light;
	light.color = glm::vec4(0.0, 0.0, 0.0, 1.0);
	light.emissionColor = glm::vec4(0.99, 0.95, 0.78, 1.0);
	light.specularColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	light.data = glm::vec4(1.5, 0.0, 0.0, 0.0);
	//light.data = glm::vec4(0.8, 0.0, 0.0, 0.0);

	Material spec;
	spec.color = glm::vec4(1.0, 0.39, 0.28, 1.0);
	spec.emissionColor = glm::vec4(0.0, 0.0, 0.0, 1.0);
	spec.specularColor = glm::vec4(1.0, 1.0, 1.0, 1.0);
	spec.data = glm::vec4(0.0, 1.0, 0.18, 0.0);

	Material diffuse;
	diffuse.color = glm::vec4(1.0, 0.5, 1.0, 1.0);
	diffuse.emissionColor = glm::vec4(1.0, 1.0, 1.0, 1.0);
	diffuse.specularColor = glm::vec4(1.0, 1.0, 1.0, 1.0);
	diffuse.data = glm::vec4(0.0, 1.0, 0.1, 0.0);

	Material ground;
	ground.color = glm::vec4(1.0, 0.9, 0.9, 1.0);
	ground.emissionColor = glm::vec4(0.0, 0.0, 0.0, 1.0);
	ground.specularColor = glm::vec4(0.0, 0.0, 0.0, 1.0);
	ground.data = glm::vec4(0.0, 0.0, 0.0, 0.0);

	Material metal;
	metal.color = glm::vec4(0.9, 0.9, 0.1, 1.0);
	metal.emissionColor = glm::vec4(0.0, 0.0, 0.0, 1.0);
	metal.specularColor = glm::vec4(1.0, 1.0, 1.0, 1.0);
	metal.data = glm::vec4(0.0, 0.9, 0.91, 0.0);

	numMaterials = matvect.size();
	cout << setw(20) << left << "# of materials: " << matvect.size() << endl;

	matvect.push_back(light);
	matvect.push_back(spec);
	matvect.push_back(diffuse);
	matvect.push_back(ground);
	matvect.push_back(metal);

	glGenBuffers(1, &materialSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, matvect.size() * sizeof(Material), NULL, GL_STATIC_DRAW);
	Material* materials = (struct Material*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, matvect.size() * sizeof(Material), bufMask);

	for (int matind = 0; matind < matvect.size(); matind++) {
		materials[matind] = matvect[matind];
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);



	numSpheres = 1;
	cout << setw(20) << left << "# of spheres: " << numSpheres << endl;

	glGenBuffers(1, &sphereSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, numSpheres * sizeof(struct Sphere), NULL, GL_STATIC_DRAW);
	Sphere* spheres = (Sphere*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, numSpheres * sizeof(Sphere), bufMask);

	Sphere l;
	l.data = glm::vec4(100.0, -15.0, 93.0, 100.0);
	l.materialData = glm::vec4(numMaterials + 0.0, 0.0, 0.0, 0.0);
	//was 11

	Sphere g;
	g.data = glm::vec4(0.0, 40.0, -1000000.0, 1000000.0);
	g.materialData = glm::vec4(numMaterials + 3.0, 0.0, 0.0, 0.0);

	Sphere s1;
	s1.data = glm::vec4(-4.0, 3.0, 2.0, 2.0);
	s1.materialData = glm::vec4(numMaterials + 1.0, 0.0, 0.0, 0.0);
	//was 12

	Sphere s2;
	s2.data = glm::vec4(4.0, 3.0, 2.0, 2.0);
	s2.materialData = glm::vec4(numMaterials + 3.0, 0.0, 0.0, 0.0);

	Sphere s3;
	s3.data = glm::vec4(0.0, 3.0, 2.0, 2.0);
	s3.materialData = glm::vec4(numMaterials + 4.0, 0.0, 0.0, 0.0);

	s3.data = glm::vec4(-0.5, 3.0, 1.0, 0.8);
	s3.materialData = glm::vec4(numMaterials + 4.0, 0.0, 0.0, 0.0);

	spheres[0] = s3;
	/*spheres[1] = g;
	spheres[2] = s1;
	spheres[3] = s2;
	spheres[4] = l;*/

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);




	glGenBuffers(1, &cameraSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(glm::vec4), NULL, GL_STATIC_DRAW);
	glm::vec4* camera_buffer_data = (glm::vec4*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 3 * sizeof(glm::vec4), bufMask);

	camera_buffer_data[0] = camera_position;
	camera_buffer_data[1] = camera_direction;
	camera_buffer_data[2] = glm::vec4(0.0); // TODO replace with data such as fov

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);



	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sphereSSbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, triangleSSbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, materialSSbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cameraSSbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, bvhSSbo);
}




#endif