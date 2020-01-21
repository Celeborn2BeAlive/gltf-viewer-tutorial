#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "utils/cameras.hpp"
#include "utils/gltf.hpp"
#include "utils/images.hpp"

#include <stb_image_write.h>
#include <tiny_gltf.h>

#define VERTEX_ATTRIB_POSITION_IDX  0
#define VERTEX_ATTRIB_NORMAL_IDX  1
#define VERTEX_ATTRIB_TEXCOORD0_IDX  2

void keyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, 1);
  }
}

int ViewerApplication::run()
{
	// Loader shaders
	const auto glslProgram =
		compileProgram({m_ShadersRootPath / m_AppName / m_vertexShader,
			m_ShadersRootPath / m_AppName / m_fragmentShader});

	const auto modelViewProjMatrixLocation =
		glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
	const auto modelViewMatrixLocation =
		glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
	const auto normalMatrixLocation =
		glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

	// Build projection matrix
	auto maxDistance = 500.f; // TODO use scene bounds instead to compute this
	maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
	const auto projMatrix =
		glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
			0.001f * maxDistance, 1.5f * maxDistance);

	// TODO Implement a new CameraController model and use it instead. Propose the
	// choice from the GUI
	FirstPersonCameraController cameraController{
		m_GLFWHandle.window(), 0.5f * maxDistance};
	if (m_hasUserCamera) {
		cameraController.setCamera(m_userCamera);
	} else {
		// TODO Use scene bounds to compute a better default camera
		cameraController.setCamera(
			Camera{glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)});
	}

	tinygltf::Model model;
	if (!loadGltfFile(model)){
		return -1;
	}

	const auto bufferObjects = createBufferObjects(model);
	std::vector<VaoRange> meshIndexToVaoRange;
	std::vector<GLuint> VAOs = createVertexArrayObjects(model, bufferObjects, meshIndexToVaoRange);

	// Setup OpenGL state for rendering
	glEnable(GL_DEPTH_TEST);
	glslProgram.use();

	// Lambda function to draw the scene
  	const auto drawScene = [&](const Camera &camera) {
		glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const auto viewMatrix = camera.getViewMatrix();

		// The recursive function that should draw a node
		// We use a std::function because a simple lambda cannot be recursive
		const std::function<void(int, const glm::mat4 &)> drawNode = [&](int nodeIdx, const glm::mat4 &parentMatrix) {
			const auto &node = model.nodes[nodeIdx];
			glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);
			int meshIdx = node.mesh;
			if (meshIdx >= 0){ //check if the node is a mesh
				glm::mat4 MV = viewMatrix * modelMatrix;
				glm::mat4 MVP = projMatrix*MV;
				glm::mat4 normalMatrix = transpose(inverse(MV));
				glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(MV));
				glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE, glm::value_ptr(MVP));
				glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));

				const auto &mesh = model.meshes[meshIdx];
				VaoRange vaoRange = meshIndexToVaoRange[meshIdx];
				const size_t primitiveNumber = mesh.primitives.size();
				for (size_t i=0; i < primitiveNumber; ++i){
					const auto &vao = VAOs[vaoRange.begin + i];
					glBindVertexArray(vao);
					const auto &currentPrimitive = mesh.primitives[i];
					if (currentPrimitive.indices >= 0){
						const auto &accessor = model.accessors[currentPrimitive.indices];
						const auto &bufferView = model.bufferViews[accessor.bufferView];
						const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
						glDrawElements(currentPrimitive.mode, accessor.count, accessor.componentType, (const GLvoid *)byteOffset);
					} else {
						const auto accessorIdx = (*begin(currentPrimitive.attributes)).second;
						const auto &accessor = model.accessors[accessorIdx];
						glDrawArrays(currentPrimitive.mode, 0, accessor.count);
					}
				}
			}
			for (const auto &nodeIdx : node.children){
				drawNode(nodeIdx, modelMatrix);
			}
		};

		// Draw the scene referenced by gltf file
		if (model.defaultScene >= 0) {
			for (const auto &nodeIdx : model.scenes[model.defaultScene].nodes){
				drawNode(nodeIdx, glm::mat4(1));
			}
		}
	};


	if (!m_OutputPath.empty()){
		std::vector<unsigned char> pixels(m_nWindowWidth*m_nWindowHeight, 0);
		ViewerApplication::renderToImage(m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), [&]() {drawScene(cameraController.getCamera());});
		flipImageYAxis<unsigned char>(m_nWindowWidth, m_nWindowHeight, 3, pixels.data());
		const auto strPath = m_OutputPath.string();
		stbi_write_png(strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);
		return 0;
	}

	// Loop until the user closes the window
	for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
		++iterationCount) {
		const auto seconds = glfwGetTime();

		const auto camera = cameraController.getCamera();
		drawScene(camera);

		// GUI code:
		imguiNewFrame();

		{
		ImGui::Begin("GUI");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
				camera.eye().z);
			ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
				camera.center().y, camera.center().z);
			ImGui::Text(
				"up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

			ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
				camera.front().z);
			ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
				camera.left().z);

			if (ImGui::Button("CLI camera args to clipboard")) {
			std::stringstream ss;
			ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
				<< camera.eye().z << "," << camera.center().x << ","
				<< camera.center().y << "," << camera.center().z << ","
				<< camera.up().x << "," << camera.up().y << "," << camera.up().z;
			const auto str = ss.str();
			glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
			}
		}
		ImGui::End();
		}

		imguiRenderFrame();

		glfwPollEvents(); // Poll for and process events

		auto ellapsedTime = glfwGetTime() - seconds;
		auto guiHasFocus =
			ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
		if (!guiHasFocus) {
		cameraController.update(float(ellapsedTime));
		}

		m_GLFWHandle.swapBuffers(); // Swap front and back buffers
	}

	// TODO clean up allocated GL data

	return 0;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
    uint32_t height, const fs::path &gltfFile,
    const std::vector<float> &lookatArgs, const std::string &vertexShader,
    const std::string &fragmentShader, const fs::path &output) :
    m_nWindowWidth(width),
    m_nWindowHeight(height),
    m_AppPath{appPath},
    m_AppName{m_AppPath.stem().string()},
    m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
    m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
    m_gltfFilePath{gltfFile},
    m_OutputPath{output}
{
  if (!lookatArgs.empty()) {
    m_hasUserCamera = true;
    m_userCamera =
        Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
            glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
            glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
  }

  if (!vertexShader.empty()) {
    m_vertexShader = vertexShader;
  }

  if (!fragmentShader.empty()) {
    m_fragmentShader = fragmentShader;
  }

  ImGui::GetIO().IniFilename =
      m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
                                  // positions in this file

  glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

  printGLVersion();
}

bool ViewerApplication::loadGltfFile(tinygltf::Model & model){
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

    if (!warn.empty()) {
        std::cout << "Warn: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
		std::cerr << "Failed to parse glTF" << std::endl;
		return false;
    }

    return ret;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(const tinygltf::Model &model){
    size_t size = model.buffers.size();
    std::vector<GLuint> bufferObjects(size, 0);
    glGenBuffers(size, bufferObjects.data());
    
    for (size_t i=0; i < size; ++i){
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
        glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(), model.buffers[i].data.data(), 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects( const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<ViewerApplication::VaoRange> & meshIndexToVaoRange){
    std::vector<GLuint> vertexArrayObjects;

    for (const auto &mesh : model.meshes){
		GLsizei primitiveNumber = mesh.primitives.size();
		GLsizei offset = vertexArrayObjects.size();
		vertexArrayObjects.resize(offset + primitiveNumber);
		meshIndexToVaoRange.push_back(ViewerApplication::VaoRange{offset, primitiveNumber});
		
		glGenVertexArrays(primitiveNumber ,vertexArrayObjects.data()+offset);

		for (size_t i=0; i < primitiveNumber; ++i){
			glBindVertexArray(vertexArrayObjects[offset + i]);
			auto currentPrimitive = mesh.primitives[i];

			{ // I'm opening a scope because I want to reuse the variable iterator in the code for NORMAL and TEXCOORD_0
				const auto iterator = currentPrimitive.attributes.find("POSITION");
				if (iterator != end(currentPrimitive.attributes)) { // If "POSITION" has been found in the map
					const auto accessorIdx = (*iterator).second; // (*iterator).first is the key "POSITION", (*iterator).second is the value, ie. the index of the accessor for this attribute
					const auto &accessor = model.accessors[accessorIdx]; //get the correct tinygltf::Accessor from model.accessors
					const auto &bufferView = model.bufferViews[accessor.bufferView]; //get the correct tinygltf::BufferView from model.bufferViews. You need to use the accessor
					const int bufferIdx = bufferView.buffer; //get the index of the buffer used by the bufferView (you need to use it)
					//const int bufferIdx = model.buffers[bufferView.buffer]; //get the index of the buffer used by the bufferView (you need to use it)
					
					const auto bufferObject = bufferObjects[bufferIdx]; //get the correct buffer object from the buffer index

					glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX); //Enable the vertex attrib array corresponding to POSITION with glEnableVertexAttribArray (you need to use VERTEX_ATTRIB_POSITION_IDX which is defined at the top of the file)
					glBindBuffer(GL_ARRAY_BUFFER, bufferObject); //Bind the buffer object to GL_ARRAY_BUFFER

					const auto byteOffset = bufferView.byteOffset + accessor.byteOffset; //Compute the total byte offset using the accessor and the buffer view
					glVertexAttribPointer(VERTEX_ATTRIB_POSITION_IDX, 3, GL_FLOAT, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)byteOffset);// TODO Call glVertexAttribPointer with the correct arguments. 
					// Remember size is obtained with accessor.type, type is obtained with accessor.componentType. 
					// The stride is obtained in the bufferView, normalized is always GL_FALSE, and pointer is the byteOffset (don't forget the cast).
				}
			}
			{
				const auto iterator = currentPrimitive.attributes.find("NORMAL");
				if (iterator != end(currentPrimitive.attributes)) { 
					const auto accessorIdx = (*iterator).second; // (*iterator).first is the key "POSITION", (*iterator).second is the value, ie. the index of the accessor for this attribute
					const auto &accessor = model.accessors[accessorIdx]; //get the correct tinygltf::Accessor from model.accessors
					const auto &bufferView = model.bufferViews[accessor.bufferView]; //get the correct tinygltf::BufferView from model.bufferViews. You need to use the accessor
					const int bufferIdx = bufferView.buffer; //get the index of the buffer used by the bufferView (you need to use it)
					//const int bufferIdx = model.buffers[bufferView.buffer]; //get the index of the buffer used by the bufferView (you need to use it)
					
					const auto bufferObject = bufferObjects[bufferIdx]; //get the correct buffer object from the buffer index

					glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL_IDX); //Enable the vertex attrib array corresponding to POSITION with glEnableVertexAttribArray (you need to use VERTEX_ATTRIB_POSITION_IDX which is defined at the top of the file)
					glBindBuffer(GL_ARRAY_BUFFER, bufferObject); //Bind the buffer object to GL_ARRAY_BUFFER

					const auto byteOffset = bufferView.byteOffset + accessor.byteOffset; //Compute the total byte offset using the accessor and the buffer view
					glVertexAttribPointer(VERTEX_ATTRIB_NORMAL_IDX, 3, GL_FLOAT, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)byteOffset);// TODO Call glVertexAttribPointer with the correct arguments. 
					// Remember size is obtained with accessor.type, type is obtained with accessor.componentType. 
					// The stride is obtained in the bufferView, normalized is always GL_FALSE, and pointer is the byteOffset (don't forget the cast).
				}
			}
			{ // I'm opening a scope because I want to reuse the variable iterator in the code for NORMAL and TEXCOORD_0
				const auto iterator = currentPrimitive.attributes.find("TEXCOORD_0");
				if (iterator != end(currentPrimitive.attributes)) { // If "POSITION" has been found in the map
					// (*iterator).first is the key "POSITION", (*iterator).second is the value, ie. the index of the accessor for this attribute
					const auto accessorIdx = (*iterator).second;
					const auto &accessor = model.accessors[accessorIdx]; //get the correct tinygltf::Accessor from model.accessors
					const auto &bufferView = model.bufferViews[accessor.bufferView]; //get the correct tinygltf::BufferView from model.bufferViews. You need to use the accessor
					const int bufferIdx = bufferView.buffer; //get the index of the buffer used by the bufferView (you need to use it)
					//const int bufferIdx = model.buffers[bufferView.buffer]; //get the index of the buffer used by the bufferView (you need to use it)
					
					const auto bufferObject = bufferObjects[bufferIdx]; //get the correct buffer object from the buffer index

					glEnableVertexAttribArray(VERTEX_ATTRIB_TEXCOORD0_IDX); //Enable the vertex attrib array corresponding to POSITION with glEnableVertexAttribArray (you need to use VERTEX_ATTRIB_POSITION_IDX which is defined at the top of the file)
					glBindBuffer(GL_ARRAY_BUFFER, bufferObject); //Bind the buffer object to GL_ARRAY_BUFFER

					const auto byteOffset = bufferView.byteOffset + accessor.byteOffset; //Compute the total byte offset using the accessor and the buffer view
					glVertexAttribPointer(VERTEX_ATTRIB_TEXCOORD0_IDX, 3, GL_FLOAT, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)byteOffset);// TODO Call glVertexAttribPointer with the correct arguments. 
					// Remember size is obtained with accessor.type, type is obtained with accessor.componentType. 
					// The stride is obtained in the bufferView, normalized is always GL_FALSE, and pointer is the byteOffset (don't forget the cast).
				}
			}
			// Index array if defined (if the primitive is use several times)
			uint primitiveIndices = currentPrimitive.indices;
			if (primitiveIndices >= 0) {
				const auto accessorIdx = primitiveIndices;
				const auto &accessor = model.accessors[accessorIdx];
				const auto &bufferView = model.bufferViews[accessor.bufferView];
				const auto bufferIdx = bufferView.buffer;

				const auto bufferObject = bufferObjects[bufferIdx];

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObject);
			}
		}

    }
	glBindVertexArray(0);
	//std::clog << "Number of VAOs: " << vertexArrayObjects.size() << std::endl;
    return vertexArrayObjects;
}