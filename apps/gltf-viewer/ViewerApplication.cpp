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

bool ViewerApplication::loadGltfFile(tinygltf::Model & model){

  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());
  //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

  if (!warn.empty()) {
    std::cerr << "Warn: " << warn << std::endl;
    return false;
  }

  if (!err.empty()) {
    std::cerr << "Err: " <<  err << std::endl;
    return false;
  }

  if (!ret) {
    std::cerr << "Failed to parse glTF" << std::endl;
    return false;
  }
  return true;
}

std::vector<GLuint> ViewerApplication::createBufferObjects( const tinygltf::Model &model) {
    std::vector<GLuint> bufferObjects(model.buffers.size(), 0);
    glGenBuffers(model.buffers.size(), bufferObjects.data());
    for (size_t bufferIdx = 0; bufferIdx < bufferObjects.size(); bufferIdx++)
    {
      glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);
      glBufferStorage(GL_ARRAY_BUFFER, model.buffers[bufferIdx].data.size(), model.buffers[bufferIdx].data.data(), 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects( const tinygltf::Model &model, 
  const std::vector<GLuint> &bufferObjects, 
  std::vector<VaoRange> &meshIndexToVaoRange) {
    std::vector<GLuint> vertexArrayObjects;

    const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
    const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
    const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;
    
    for(int meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++ ) {
        const int vaoOffset = vertexArrayObjects.size();
        const int primitiveSizeRange = model.meshes[meshIdx].primitives.size();
        vertexArrayObjects.resize(vaoOffset + primitiveSizeRange);
        meshIndexToVaoRange.push_back(VaoRange{vaoOffset, primitiveSizeRange});
        glGenVertexArrays(primitiveSizeRange, &vertexArrayObjects[meshIdx]);

        for(int primitiveIdx = 0; primitiveIdx < primitiveSizeRange; primitiveIdx++) {
          glBindVertexArray(vertexArrayObjects[vaoOffset + primitiveIdx]);
          {
            const auto iterator = model.meshes[meshIdx].primitives[primitiveIdx].attributes.find("POSITION");
            if (iterator != end(model.meshes[meshIdx].primitives[primitiveIdx].attributes)) {
              const auto accessorIdx = (*iterator).second;
              const auto &accessor = model.accessors[accessorIdx];
              const auto &bufferView = model.bufferViews[accessor.bufferView]; 
              const auto bufferIdx = bufferView.buffer;

              const auto bufferObject = bufferObjects[bufferIdx];

              glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX);
              glBindBuffer(GL_ARRAY_BUFFER, bufferObject);

              const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
              glVertexAttribPointer(VERTEX_ATTRIB_POSITION_IDX, accessor.type, accessor.componentType, GL_FALSE, bufferView.byteStride, (void *)byteOffset);
            }
          }
          {
            const auto iterator = model.meshes[meshIdx].primitives[primitiveIdx].attributes.find("NORMAL");
            if (iterator != end(model.meshes[meshIdx].primitives[primitiveIdx].attributes)) {
              const auto accessorIdx = (*iterator).second;
              const auto &accessor = model.accessors[accessorIdx];
              const auto &bufferView = model.bufferViews[accessor.bufferView];
              const auto bufferIdx = bufferView.buffer;

              const auto bufferObject = bufferObjects[bufferIdx];

              glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL_IDX);
              glBindBuffer(GL_ARRAY_BUFFER, bufferObject);

              const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
              glVertexAttribPointer(VERTEX_ATTRIB_NORMAL_IDX, accessor.type, accessor.componentType, GL_FALSE, bufferView.byteStride, (void *)byteOffset);
            }
          }
          {
            const auto iterator = model.meshes[meshIdx].primitives[primitiveIdx].attributes.find("TEXCOORD_0");
            if (iterator != end(model.meshes[meshIdx].primitives[primitiveIdx].attributes)) {
              const auto accessorIdx = (*iterator).second;
              const auto &accessor = model.accessors[accessorIdx];
              const auto &bufferView = model.bufferViews[accessor.bufferView];
              const auto bufferIdx = bufferView.buffer;

              const auto bufferObject = bufferObjects[bufferIdx];

              glEnableVertexAttribArray(VERTEX_ATTRIB_TEXCOORD0_IDX);
              glBindBuffer(GL_ARRAY_BUFFER, bufferObject);

              const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
              glVertexAttribPointer(VERTEX_ATTRIB_TEXCOORD0_IDX, accessor.type, accessor.componentType, GL_FALSE, bufferView.byteStride, (void *)byteOffset);
            }
          }
          if(model.meshes[meshIdx].primitives[primitiveIdx].indices >= 0) {
            const auto &accessor = model.accessors[model.meshes[meshIdx].primitives[primitiveIdx].indices];
              const auto &bufferView = model.bufferViews[accessor.bufferView]; 
              const auto bufferIdx = bufferView.buffer;

              const auto bufferObject = bufferObjects[bufferIdx];
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObject);
          }
        }
    }
    glBindVertexArray(0);
    return vertexArrayObjects;
}

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

  tinygltf::Model model;
  if(!loadGltfFile(model)) {
    return EXIT_FAILURE;
  };

  
  glm::vec3 bboxMin, bboxMax;
  computeSceneBounds(model, bboxMin, bboxMax);
  const glm::vec3 center = (bboxMax + bboxMin) * 0.5f;
  const glm::vec3 diagonalVector = bboxMax - bboxMin;
  const glm::vec3 up(0, 1, 0);
  const glm::vec3 eye = diagonalVector.z > 0 ? center + diagonalVector : 
                                              center + 2.f * glm::cross(diagonalVector, up);
  // Build projection matrix
  auto maxDistance = glm::length(diagonalVector);
  maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  // TODO Implement a new CameraController model and use it instead. Propose the
  // choice from the GUI
  TrackballCameraController cameraController{
      m_GLFWHandle.window(), 3.f * maxDistance};
  if (m_hasUserCamera) {
    cameraController.setCamera(m_userCamera);
  } else {
    cameraController.setCamera(
        Camera{eye, center, up});
  }

  std::vector<GLuint> bufferObjects = createBufferObjects(model);

  std::vector<VaoRange> meshIndexToVaoRange; 
  std::vector<GLuint> vertexArrayObjects = createVertexArrayObjects(model, bufferObjects, meshIndexToVaoRange);
  //std::cout << vertexArrayObjects.size() << std::endl;

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
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix) {
          tinygltf::Node node = model.nodes[nodeIdx];
          glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);
          if(node.mesh >= 0){
            const glm::mat4 modelViewMatrix = modelMatrix * viewMatrix;
            const glm::mat4 modelViewProjectionMatrix = projMatrix * modelViewMatrix;
            const glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));

            glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, value_ptr(modelViewMatrix));
            glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE, value_ptr(modelViewProjectionMatrix));
            glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, value_ptr(normalMatrix));

            const tinygltf::Mesh &mesh = model.meshes[node.mesh];
            const auto &vaoRangeMesh = meshIndexToVaoRange[node.mesh];
            for(size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
              const auto vaoPrimitive = vertexArrayObjects[vaoRangeMesh.begin + primIdx];
              const auto &currentPrimitive = mesh.primitives[primIdx];
              glBindVertexArray(vaoPrimitive);
              if(currentPrimitive.indices >= 0) {
                const auto &accessor = model.accessors[currentPrimitive.indices];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                glDrawElements(currentPrimitive.mode, GLsizei(accessor.count),
                    accessor.componentType, (const GLvoid *)byteOffset);
              } else {
                const auto accessorIdx = (*begin(currentPrimitive.attributes)).second;
                const auto &accessor = model.accessors[accessorIdx];
                glDrawArrays(currentPrimitive.mode, 0, GLsizei(accessor.count));
              }
            }
          }
          for(const auto childNode : node.children) {
            drawNode(childNode, modelMatrix);
          }
        };

    // Draw the scene referenced by gltf file
    if (model.defaultScene >= 0) {
      for(int nodeIdx : model.scenes[model.defaultScene].nodes) {
        drawNode(model.scenes[model.defaultScene].nodes[nodeIdx], glm::mat4(1));
      }
    }
  };

  if(!m_OutputPath.empty()) {
    size_t numCoponents = 3;
    std::vector<unsigned char> pixels(numCoponents * m_nWindowWidth * m_nWindowHeight);
    renderToImage(m_nWindowWidth, m_nWindowHeight, numCoponents, pixels.data(), [&]() {
      drawScene(cameraController.getCamera());
    });
    flipImageYAxis(m_nWindowWidth, m_nWindowHeight, numCoponents, pixels.data());
    const auto strPath = m_OutputPath.string();
    stbi_write_png(
    strPath.c_str(), m_nWindowWidth, m_nWindowHeight, numCoponents, pixels.data(), 0);
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