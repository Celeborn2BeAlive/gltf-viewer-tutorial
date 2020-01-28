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

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        glfwSetWindowShouldClose(window, 1);
    }
}

bool ViewerApplication::loadGltfFile(tinygltf::Model &model) { // TODO Loading the glTF file
    std::clog << "Loading file " << m_gltfFilePath << std::endl;

    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // m_gltfFilePath.string() au lieu de argv[1]
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

    if (!warn.empty()) {
        std::cerr << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        std::cerr << "Failed to parse glTF file" << std::endl;
        return false;
    }

    return true;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(const tinygltf::Model &model) { // TODO Creation of Buffer Objects
    std::vector<GLuint> bufferObjects(model.buffers.size(), 0);

    glGenBuffers(GLsizei(model.buffers.size()), bufferObjects.data());
    for (size_t i = 0; i < model.buffers.size(); ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
        glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(), model.buffers[i].data.data(), 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects,
                                                                std::vector<VaoRange> &meshToVertexArrays) {  // TODO Creation of Vertex Array Objects
    std::vector<GLuint> vertexArrayObjects; // We don't know the size yet

    // For each mesh of model we keep its range of VAOs
    meshToVertexArrays.resize(model.meshes.size());

    const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
    const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
    const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;

    for (size_t i = 0; i < model.meshes.size(); ++i) {
        const auto &mesh = model.meshes[i];

        auto &vaoRange = meshToVertexArrays[i];
        vaoRange.begin = GLsizei(vertexArrayObjects.size()); // Range for this mesh will be at
        // the end of vertexArrayObjects
        vaoRange.count = GLsizei(mesh.primitives.size()); // One VAO for each primitive

        // Add enough elements to store our VAOs identifiers
        vertexArrayObjects.resize(vertexArrayObjects.size() + mesh.primitives.size());

        glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);
        for (size_t pIdx = 0; pIdx < mesh.primitives.size(); ++pIdx) {
            const auto vao = vertexArrayObjects[vaoRange.begin + pIdx];
            const auto &primitive = mesh.primitives[pIdx];
            glBindVertexArray(vao);
            { // POSITION attribute
                // scope, so we can declare const variable with the same name on each
                // scope
                const auto iterator = primitive.attributes.find("POSITION");
                if (iterator != end(primitive.attributes)) {
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessor.bufferView];
                    const auto bufferIdx = bufferView.buffer;

                    glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX);
                    assert(GL_ARRAY_BUFFER == bufferView.target);
                    // Theorically we could also use bufferView.target, but it is safer
                    // Here it is important to know that the next call
                    // (glVertexAttribPointer) use what is currently bound
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);

                    // tinygltf converts strings type like "VEC3, "VEC2" to the number of
                    // components, stored in accessor.type
                    const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                    glVertexAttribPointer(VERTEX_ATTRIB_POSITION_IDX, accessor.type, accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)byteOffset);
                }
            }
            // todo Refactor to remove code duplication (loop over "POSITION",
            // "NORMAL" and their corresponding VERTEX_ATTRIB_*)
            { // NORMAL attribute
                const auto iterator = primitive.attributes.find("NORMAL");
                if (iterator != end(primitive.attributes)) {
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessor.bufferView];
                    const auto bufferIdx = bufferView.buffer;

                    glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL_IDX);
                    assert(GL_ARRAY_BUFFER == bufferView.target);
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);
                    glVertexAttribPointer(VERTEX_ATTRIB_NORMAL_IDX, accessor.type, accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)(accessor.byteOffset + bufferView.byteOffset));
                }
            }
            { // TEXCOORD_0 attribute
                const auto iterator = primitive.attributes.find("TEXCOORD_0");
                if (iterator != end(primitive.attributes)) {
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessor.bufferView];
                    const auto bufferIdx = bufferView.buffer;

                    glEnableVertexAttribArray(VERTEX_ATTRIB_TEXCOORD0_IDX);
                    assert(GL_ARRAY_BUFFER == bufferView.target);
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);
                    glVertexAttribPointer(VERTEX_ATTRIB_TEXCOORD0_IDX, accessor.type, accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride), (const GLvoid *)(accessor.byteOffset + bufferView.byteOffset));
                }
            }
            // Index array if defined
            if (primitive.indices >= 0) {
                const auto accessorIdx = primitive.indices;
                const auto &accessor = model.accessors[accessorIdx];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto bufferIdx = bufferView.buffer;

                assert(GL_ELEMENT_ARRAY_BUFFER == bufferView.target);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[bufferIdx]); // Binding the index buffer to
                // GL_ELEMENT_ARRAY_BUFFER while the VAO
                // is bound is enough to tell OpenGL we
                // want to use that index buffer for that
                // VAO
            }
        }
    }
    glBindVertexArray(0);
    std::clog << "Number of VAOs: " << vertexArrayObjects.size() << std::endl;
    return vertexArrayObjects;
}

int ViewerApplication::run() {
    // Loader shaders
    const auto glslProgram = compileProgram({ m_ShadersRootPath / m_AppName / m_vertexShader,
                                              m_ShadersRootPath / m_AppName / m_fragmentShader });

    const auto modelViewProjMatrixLocation = glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
    const auto modelViewMatrixLocation = glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
    const auto normalMatrixLocation = glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

    // Build projection matrix
    auto maxDistance = 500.f; // TODO use scene bounds instead to compute this
    maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
    const auto projMatrix = glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight, 0.001f * maxDistance, 1.5f * maxDistance);

    // TODO Implement a new CameraController model and use it instead. Propose the
    // choice from the GUI
    FirstPersonCameraController cameraController{ m_GLFWHandle.window(), 0.5f * maxDistance };
    if (m_hasUserCamera) {
        cameraController.setCamera(m_userCamera);
    } 
    else {
        // TODO Use scene bounds to compute a better default camera
        cameraController.setCamera(Camera{ glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0) });
    }

    tinygltf::Model model;
    // TODO Loading the glTF file
    if (!loadGltfFile(model)) {
        return -1;
    }

    // TODO Creation of Buffer Objects
    const auto bufferObjects = createBufferObjects(model);

    // TODO Creation of Vertex Array Objects
    std::vector<VaoRange> meshToVertexArrays;
    const auto vertexArrayObjects = createVertexArrayObjects(model, bufferObjects, meshToVertexArrays);

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
            // TODO The drawNode function
            const auto &node = model.nodes[nodeIdx];
            const glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);

            // If the node references a mesh (a node can also reference a
            // camera, or a light)
            if (node.mesh >= 0) {
                // Also called localToCamera matrix
                const auto mvMatrix = viewMatrix * modelMatrix;
                // Also called localToScreen matrix
                const auto mvpMatrix = projMatrix * mvMatrix;
                // Normal matrix is necessary to maintain normal vectors
                // orthogonal to tangent vectors
                const auto normalMatrix = glm::transpose(glm::inverse(mvMatrix));

                glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvMatrix));
                glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));

                const auto &mesh = model.meshes[node.mesh];
                const auto &vaoRange = meshToVertexArrays[node.mesh];
                for (int i = 0; i < mesh.primitives.size(); ++i) {
                    const auto vao = vertexArrayObjects[vaoRange.begin + i];
                    const auto &primitive = mesh.primitives[i];
                    glBindVertexArray(vao);

                    if (primitive.indices >= 0) {
                        const auto &accessor = model.accessors[primitive.indices];
                        const auto &bufferView = model.bufferViews[accessor.bufferView];
                        const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                        glDrawElements(primitive.mode, GLsizei(accessor.count), accessor.componentType, (const GLvoid *)byteOffset);
                    } 
                    else { // Take first accessor to get the count
                        const auto accessorIdx = (*begin(primitive.attributes)).second;
                        const auto &accessor = model.accessors[accessorIdx];
                        glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
                    }
                }
            }
            // Draw children
            for (auto nodeChild : node.children) {
                drawNode(nodeChild, modelMatrix);
            }
        };

        // Draw the scene referenced by gltf file
        if (model.defaultScene >= 0) {
            // TODO Draw all nodes
            for (const auto nodeIdx : model.scenes[model.defaultScene].nodes) {
                drawNode(nodeIdx, glm::mat4(1));
            }
        }
    };

    // Loop until the user closes the window
    if (!(m_OutputPath.empty())) {
        const auto nbComponent = 3;
        std::vector<unsigned char> pixels(m_nWindowWidth * m_nWindowHeight * nbComponent);
        renderToImage(m_nWindowWidth, m_nWindowHeight, nbComponent, pixels.data(), [&]() {
            drawScene(cameraController.getCamera());
        });
        flipImageYAxis(m_nWindowWidth, m_nWindowHeight, nbComponent, pixels.data());

        const auto strPath = m_OutputPath.string();
        stbi_write_png(strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);

        return 0;
    }

    for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose(); ++iterationCount) {
        const auto seconds = glfwGetTime();
        const auto camera = cameraController.getCamera();
        drawScene(camera);

        // GUI code:
        imguiNewFrame();
        {
            ImGui::Begin("GUI");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y, camera.eye().z);
                ImGui::Text("center: %.3f %.3f %.3f", camera.center().x, camera.center().y, camera.center().z);
                ImGui::Text("up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);
                ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y, camera.front().z);
                ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y, camera.left().z);

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
        auto guiHasFocus = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
        if (!guiHasFocus) {
            cameraController.update(float(ellapsedTime));
        }
        m_GLFWHandle.swapBuffers(); // Swap front and back buffers
    }
    // TODO clean up allocated GL data

    return 0;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width, uint32_t height, const fs::path &gltfFile,
                                     const std::vector<float> &lookatArgs, const std::string &vertexShader, const std::string &fragmentShader, 
                                     const fs::path &output) : m_nWindowWidth(width), m_nWindowHeight(height), m_AppPath{appPath}, 
                                     m_AppName{m_AppPath.stem().string()}, m_ImGuiIniFilename{m_AppName + ".imgui.ini"}, 
                                     m_ShadersRootPath{m_AppPath.parent_path() / "shaders"}, m_gltfFilePath{gltfFile}, m_OutputPath{output} {
    if (!lookatArgs.empty()) {
        m_hasUserCamera = true;
        m_userCamera = Camera { glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
                                glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
                                glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
    }

    if (!vertexShader.empty()) {
        m_vertexShader = vertexShader;
    }

    if (!fragmentShader.empty()) {
        m_fragmentShader = fragmentShader;
    }

    ImGui::GetIO().IniFilename = m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
    // positions in this file
    glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);
    printGLVersion();
}