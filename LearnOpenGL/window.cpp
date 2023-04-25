

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>
#include <shader_c.h>
#include <camera.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void renderQuad();

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// texture size
const unsigned int TEXTURE_WIDTH = 1000, TEXTURE_HEIGHT = 1000;

// timing 
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f; // time of last frame

struct Material
{
	glm::vec4 color;
	glm::vec4 emissionColor;
	glm::vec4 specularColor;

	glm::vec4 data; //{emissionStrength, smoothness, specularProbability, unused}
};

struct Sphere
{
	glm::vec4 data;
	glm::vec4 materialData;
};

int main(int argc, char* argv[])
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

	std::cout << "OpenGL Limitations: " << std::endl;
	std::cout << "maximum number of work groups in X dimension " << max_compute_work_group_count[0] << std::endl;
	std::cout << "maximum number of work groups in Y dimension " << max_compute_work_group_count[1] << std::endl;
	std::cout << "maximum number of work groups in Z dimension " << max_compute_work_group_count[2] << std::endl;

	std::cout << "maximum size of a work group in X dimension " << max_compute_work_group_size[0] << std::endl;
	std::cout << "maximum size of a work group in Y dimension " << max_compute_work_group_size[1] << std::endl;
	std::cout << "maximum size of a work group in Z dimension " << max_compute_work_group_size[2] << std::endl;

	std::cout << "Number of invocations in a single local work group that may be dispatched to a compute shader " << max_compute_work_group_invocations << std::endl;

	// build and compile shaders
	// -------------------------
	Shader screenQuad("screenQuad.vs", "screenQuad.fs");
	ComputeShader computeShader("computeShader.cs");

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

	//I have no idea what I am doing

	GLuint posSSbo;
	GLuint velSSbo;

	glGenBuffers(1, &posSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, posSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 5 * sizeof(struct Sphere), NULL, GL_STATIC_DRAW);
	GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; // the invalidate makes a big difference when re-writing
	Sphere* spheres = (Sphere*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 5 * sizeof(Sphere), bufMask);

	Sphere l;
	l.data = glm::vec4(100.0, -15.0, 93.0, 100.0);
	l.materialData = glm::vec4(0.0, 0.0, 0.0, 0.0);

	Sphere g;
	g.data = glm::vec4(0.0, 40.0, -45007.0, 45000.0);
	g.materialData = glm::vec4(3.0, 0.0, 0.0, 0.0);

	Sphere s1;
	s1.data = glm::vec4(0.0, 40.0, -2.0, 5.0);
	s1.materialData = glm::vec4(1.0, 0.0, 0.0, 0.0);

	Sphere s2;
	s2.data = glm::vec4(10.0, 40.0, -2.0, 5.0);
	s2.materialData = glm::vec4(4.0, 0.0, 0.0, 0.0);

	Sphere s3;
	s3.data = glm::vec4(-10.0, 40.0, -2.0, 5.0);
	s3.materialData = glm::vec4(2.0, 0.0, 0.0, 0.0);

	spheres[0] = l;
	spheres[1] = g;
	spheres[2] = s1;
	spheres[3] = s2;
	spheres[4] = s3;

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	Material light;
	light.color = glm::vec4(0.0, 0.0, 0.0, 0.0);
	light.emissionColor = glm::vec4(0.99, 0.95, 0.78, 0.0);
	light.specularColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	light.data = glm::vec4(1.5, 0.0, 0.0, 0.0);

	Material spec;
	spec.color = glm::vec4(0.2, 0.2, 1.0, 0.0);
	spec.emissionColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	spec.specularColor = glm::vec4(1.0, 1.0, 1.0, 0.0);
	spec.data = glm::vec4(0.0, 1.0, 0.2, 0.0);

	Material diffuse;
	diffuse.color = glm::vec4(1.0, 0.5, 1.0, 0.0);
	diffuse.emissionColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	diffuse.specularColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	diffuse.data = glm::vec4(0.0, 0.0, 0.0, 0.0);

	Material ground;
	ground.color = glm::vec4(1.0, 0.9, 0.9, 0.0);
	ground.emissionColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	ground.specularColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	ground.data = glm::vec4(0.0, 0.0, 0.0, 0.0);

	Material metal;
	metal.color = glm::vec4(0.9, 0.9, 0.1, 0.0);
	metal.emissionColor = glm::vec4(0.0, 0.0, 0.0, 0.0);
	metal.specularColor = glm::vec4(1.0, 1.0, 1.0, 0.0);
	metal.data = glm::vec4(0.0, 0.99, 0.91, 0.0);

	glGenBuffers(1, &velSSbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, velSSbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 5 * sizeof(Material), NULL, GL_STATIC_DRAW);
	Material* materials = (struct Material*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 5 * sizeof(Material), bufMask);
	
	materials[0] = light;
	materials[1] = spec;
	materials[2] = diffuse;
	materials[3] = ground;
	materials[4] = metal;

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, posSSbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, velSSbo);

	// render loop
	// -----------
	int fCounter = 0;
	int frameCount = 0;
	while (!glfwWindowShouldClose(window))
	{
		frameCount++;
		// Set frame time
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		if (fCounter > 40) {
			std::cout << "\rFPS: " << 1 / deltaTime << '    ' << std::flush;
			fCounter = 0;
		}
		else {
			fCounter++;
		}

		//this is maybe how to write to buffer

		spheres[2].data.z = 2.0 * sin(currentFrame * 3.0) - 2.0;
		spheres[3].data.z = 2.0 * cos(currentFrame * 3.0) - 2.0;
		spheres[4].data.z = 2.0 * sin(currentFrame * 3.0 + 3.1415) - 2.0;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, posSSbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 5 * sizeof(struct Sphere), spheres, GL_STATIC_DRAW); //sizeof(data) only works for statically sized C/C++ arrays.
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, posSSbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

		//end my weird code

		computeShader.use();
		computeShader.setFloat("t", currentFrame);
		computeShader.setFloat("frame", frameCount);
		glDispatchCompute((unsigned int)TEXTURE_WIDTH / 10, (unsigned int)TEXTURE_HEIGHT / 10, 1);

		// make sure writing to image has finished before read
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// render image to quad
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		screenQuad.use();

		renderQuad();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
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

