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

  const auto uLightDirectionLocation =
      glGetUniformLocation(glslProgram.glId(), "uLightDirection");
  const auto uLightIntensity =
      glGetUniformLocation(glslProgram.glId(), "uLightIntensity");

  const auto uBaseColorTexture =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorTexture");
  const auto uBaseColorFactor =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorFactor");

  const auto uMetallicRoughnessTexture =
      glGetUniformLocation(glslProgram.glId(), "uMetallicRoughnessTexture");
  const auto uMetallicFactor =
      glGetUniformLocation(glslProgram.glId(), "uMetallicFactor");
  const auto uRoughnessFactor =
      glGetUniformLocation(glslProgram.glId(), "uRoughnessFactor");

  tinygltf::Model model;
  if (!loadGltfFile(model)) {
    return -1;
  }
  glm::vec3 bboxMin, bboxMax;
  computeSceneBounds(model, bboxMin, bboxMax);

  // Build projection matrix
  const auto diag = bboxMax - bboxMin;
  auto maxDistance = glm::length(diag);
  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  std::unique_ptr<CameraController> cameraController =
      std::make_unique<TrackballCameraController>(
          m_GLFWHandle.window(), 0.5f * maxDistance);
  if (m_hasUserCamera) {
    cameraController->setCamera(m_userCamera);
  } else {
    const auto center = 0.5f * (bboxMax + bboxMin);
    const auto up = glm::vec3(0, 1, 0);
    const auto eye =
        diag.z > 0 ? center + diag : center + 2.f * glm::cross(diag, up);
    cameraController->setCamera(Camera{eye, center, up});
  }

  // Init light parameters
  glm::vec3 lightDirection(1, 1, 1);
  glm::vec3 lightIntensity(1, 1, 1);
  bool lightFromCamera = false;

  // Load textures
  const auto textureObjects = createTextureObjects(model);

  GLuint whiteTexture = 0;

  // Create white texture for object with no base color texture
  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  float white[] = {1, 1, 1, 1};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, white);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Load buffers
  const auto bufferObjects = createBufferObjects(model);

  std::vector<VaoRange> meshToVertexArrays;
  const auto vertexArrayObjects =
      createVertexArrayObjects(model, bufferObjects, meshToVertexArrays);

  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);
  glslProgram.use();

  const auto bindMaterial = [&](const auto materialIndex) {
    if (materialIndex >= 0) {
      const auto &material = model.materials[materialIndex];
      const auto &pbrMetallicRoughness = material.pbrMetallicRoughness;
      if (uBaseColorFactor >= 0) {
        glUniform4f(uBaseColorFactor,
            (float)pbrMetallicRoughness.baseColorFactor[0],
            (float)pbrMetallicRoughness.baseColorFactor[1],
            (float)pbrMetallicRoughness.baseColorFactor[2],
            (float)pbrMetallicRoughness.baseColorFactor[3]);
      }
      if (uBaseColorTexture >= 0) {
        auto textureObject = whiteTexture;
        if (pbrMetallicRoughness.baseColorTexture.index >= 0) {
          const auto &texture =
              model.textures[pbrMetallicRoughness.baseColorTexture.index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(uBaseColorTexture, 0);
      }
      if (uMetallicFactor >= 0) {
        glUniform1f(
            uMetallicFactor, (float)pbrMetallicRoughness.metallicFactor);
      }
      if (uRoughnessFactor >= 0) {
        glUniform1f(
            uRoughnessFactor, (float)pbrMetallicRoughness.roughnessFactor);
      }
      if (uMetallicRoughnessTexture > 0) {
        auto textureObject = 0u;
        if (pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
          const auto &texture =
              model.textures[pbrMetallicRoughness.metallicRoughnessTexture
                                 .index];
          if (texture.source >= 0) {
            textureObject = textureObjects[texture.source];
          }
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureObject);
        glUniform1i(uMetallicRoughnessTexture, 1);
      }
    } else {
      // Apply default material
      // Defined here:
      // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-material
      // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-pbrmetallicroughness3
      if (uBaseColorFactor >= 0) {
        glUniform4f(uBaseColorFactor, 1, 1, 1, 1);
      }
      if (uBaseColorTexture >= 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTexture);
        glUniform1i(uBaseColorTexture, 0);
      }
      if (uMetallicFactor >= 0) {
        glUniform1f(uMetallicFactor, 1.f);
      }
      if (uRoughnessFactor >= 0) {
        glUniform1f(uRoughnessFactor, 1.f);
      }
      if (uMetallicRoughnessTexture > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(uMetallicRoughnessTexture, 1);
      }
    }
  };

  // Lambda function to draw the scene
  const auto drawScene = [&](const Camera &camera) {
    glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto viewMatrix = camera.getViewMatrix();

    if (uLightDirectionLocation >= 0) {
      if (lightFromCamera) {
        glUniform3f(uLightDirectionLocation, 0, 0, 1);
      } else {
        const auto lightDirectionInViewSpace = glm::normalize(
            glm::vec3(viewMatrix * glm::vec4(lightDirection, 0.)));
        glUniform3f(uLightDirectionLocation, lightDirectionInViewSpace[0],
            lightDirectionInViewSpace[1], lightDirectionInViewSpace[2]);
      }
    }

    if (uLightIntensity >= 0) {
      glUniform3f(uLightIntensity, lightIntensity[0], lightIntensity[1],
          lightIntensity[2]);
    }

    // The recursive function that should draw a node
    // We use a std::function because a simple lambda cannot be recursive
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix) {
          const auto &node = model.nodes[nodeIdx];
          const glm::mat4 modelMatrix =
              getLocalToWorldMatrix(node, parentMatrix);

          // If the node references a mesh (a node can also reference a
          // camera, or a light)
          if (node.mesh >= 0) {
            const auto mvMatrix =
                viewMatrix * modelMatrix; // Also called localToCamera matrix
            const auto mvpMatrix =
                projMatrix * mvMatrix; // Also called localToScreen matrix
            // Normal matrix is necessary to maintain normal vectors
            // orthogonal to tangent vectors
            // https://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
            const auto normalMatrix = glm::transpose(glm::inverse(mvMatrix));

            glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE,
                glm::value_ptr(mvpMatrix));
            glUniformMatrix4fv(
                modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvMatrix));
            glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE,
                glm::value_ptr(normalMatrix));

            const auto &mesh = model.meshes[node.mesh];
            const auto &vaoRange = meshToVertexArrays[node.mesh];
            for (size_t pIdx = 0; pIdx < mesh.primitives.size(); ++pIdx) {
              const auto vao = vertexArrayObjects[vaoRange.begin + pIdx];
              const auto &primitive = mesh.primitives[pIdx];

              bindMaterial(primitive.material);

              glBindVertexArray(vao);
              if (primitive.indices >= 0) {
                const auto &accessor = model.accessors[primitive.indices];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto byteOffset =
                    accessor.byteOffset + bufferView.byteOffset;
                glDrawElements(primitive.mode, GLsizei(accessor.count),
                    accessor.componentType, (const GLvoid *)byteOffset);
              } else {
                // Take first accessor to get the count
                const auto accessorIdx = (*begin(primitive.attributes)).second;
                const auto &accessor = model.accessors[accessorIdx];
                glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
              }
            }
          }

          // Draw children
          for (const auto childNodeIdx : node.children) {
            drawNode(childNodeIdx, modelMatrix);
          }
        };

    // Draw the scene referenced by gltf file
    if (model.defaultScene >= 0) {
      for (const auto nodeIdx : model.scenes[model.defaultScene].nodes) {
        drawNode(nodeIdx, glm::mat4(1));
      }
    }
  };

  // If we want to render in an image
  if (!m_OutputPath.empty()) {
    const auto numComponents = 3;
    std::vector<unsigned char> pixels(
        m_nWindowWidth * m_nWindowHeight * numComponents);
    renderToImage(
        m_nWindowWidth, m_nWindowHeight, numComponents, pixels.data(), [&]() {
          const auto camera = cameraController->getCamera();
          drawScene(camera);
        });
    // OpenGL has not the same convention for image axis than most image
    // formats, so we flip on the Y axis
    flipImageYAxis(
        m_nWindowWidth, m_nWindowHeight, numComponents, pixels.data());

    // Write png on disk
    const auto strPath = m_OutputPath.string();
    stbi_write_png(
        strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);

    return 0; // Exit, in that mode we don't want to run interactive
              // viewer
  }

  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount) {
    const auto seconds = glfwGetTime();

    const auto camera = cameraController->getCamera();
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

        static int cameraControllerType = 0;
        const auto cameraControllerTypeChanged =
            ImGui::RadioButton("Trackball", &cameraControllerType, 0) ||
            ImGui::RadioButton("First Person", &cameraControllerType, 1);
        if (cameraControllerTypeChanged) {
          const auto currentCamera = cameraController->getCamera();
          if (cameraControllerType == 0) {
            cameraController = std::make_unique<TrackballCameraController>(
                m_GLFWHandle.window(), 0.5f * maxDistance);
          } else {
            cameraController = std::make_unique<FirstPersonCameraController>(
                m_GLFWHandle.window(), 0.5f * maxDistance);
          }
          cameraController->setCamera(currentCamera);
        }
      }
      if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float lightTheta = 0.f;
        static float lightPhi = 0.f;

        if (ImGui::SliderFloat("theta", &lightTheta, 0, glm::pi<float>()) ||
            ImGui::SliderFloat("phi", &lightPhi, 0, 2.f * glm::pi<float>())) {
          const auto sinPhi = glm::sin(lightPhi);
          const auto cosPhi = glm::cos(lightPhi);
          const auto sinTheta = glm::sin(lightTheta);
          const auto cosTheta = glm::cos(lightTheta);
          lightDirection =
              glm::vec3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
        }

        static glm::vec3 lightColor(1.f, 1.f, 1.f);
        static float lightIntensityFactor = 1.f;

        if (ImGui::ColorEdit3("color", (float *)&lightColor) ||
            ImGui::InputFloat("intensity", &lightIntensityFactor)) {
          lightIntensity = lightColor * lightIntensityFactor;
        }

        ImGui::Checkbox("light from camera", &lightFromCamera);
      }
      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController->update(float(ellapsedTime));
    }

    m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  // TODO clean up allocated GL data

  return 0;
}

bool ViewerApplication::loadGltfFile(tinygltf::Model &model)
{
  std::clog << "Loading file " << m_gltfFilePath << std::endl;

  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret =
      loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

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

// Texture handling could be optimized by separation of images and texture
// samplers
std::vector<GLuint> ViewerApplication::createTextureObjects(
    const tinygltf::Model &model) const
{
  std::vector<GLuint> textureObjects(model.textures.size(), 0);

  // default sampler:
  // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#texturesampler
  // "When undefined, a sampler with repeat wrapping and auto filtering should
  // be used."
  tinygltf::Sampler defaultSampler;
  defaultSampler.minFilter = GL_LINEAR;
  defaultSampler.magFilter = GL_LINEAR;
  defaultSampler.wrapS = GL_REPEAT;
  defaultSampler.wrapT = GL_REPEAT;
  defaultSampler.wrapR = GL_REPEAT;

  glActiveTexture(GL_TEXTURE0);

  glGenTextures(GLsizei(model.textures.size()), textureObjects.data());
  for (size_t i = 0; i < model.textures.size(); ++i) {
    const auto &texture = model.textures[i];
    assert(texture.source >= 0);
    const auto &image = model.images[texture.source];

    const auto &sampler =
        texture.sampler >= 0 ? model.samplers[texture.sampler] : defaultSampler;
    glBindTexture(GL_TEXTURE_2D, textureObjects[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
        GL_RGBA, image.pixel_type, image.image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, sampler.wrapR);

    if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
        sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
        sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR) {
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  return textureObjects;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(
    const tinygltf::Model &model) const
{
  std::vector<GLuint> bufferObjects(model.buffers.size(), 0);

  glGenBuffers(GLsizei(model.buffers.size()), bufferObjects.data());
  for (size_t i = 0; i < model.buffers.size(); ++i) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
    glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(),
        model.buffers[i].data.data(), 0);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects(
    const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects,
    std::vector<VaoRange> &meshToVertexArrays) const
{
  std::vector<GLuint> vertexArrayObjects; // We don't know the size yet

  // For each mesh of model we keep its range of VAOs
  meshToVertexArrays.resize(model.meshes.size());

  const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
  const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
  const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;

  for (size_t i = 0; i < model.meshes.size(); ++i) {
    const auto &mesh = model.meshes[i];

    auto &vaoRange = meshToVertexArrays[i];
    vaoRange.begin =
        GLsizei(vertexArrayObjects.size()); // Range for this mesh will be at
                                            // the end of vertexArrayObjects
    vaoRange.count =
        GLsizei(mesh.primitives.size()); // One VAO for each primitive

    // Add enough elements to store our VAOs identifiers
    vertexArrayObjects.resize(
        vertexArrayObjects.size() + mesh.primitives.size());

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

          // tinygltf converts strings type like "VEC3, "VEC2" to the number
          // of components, stored in accessor.type
          const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
          glVertexAttribPointer(VERTEX_ATTRIB_POSITION_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)byteOffset);
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
          glVertexAttribPointer(VERTEX_ATTRIB_NORMAL_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)(accessor.byteOffset + bufferView.byteOffset));
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
          glVertexAttribPointer(VERTEX_ATTRIB_TEXCOORD0_IDX, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)(accessor.byteOffset + bufferView.byteOffset));
        }
      }
      // Index array if defined
      if (primitive.indices >= 0) {
        const auto accessorIdx = primitive.indices;
        const auto &accessor = model.accessors[accessorIdx];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto bufferIdx = bufferView.buffer;

        assert(GL_ELEMENT_ARRAY_BUFFER == bufferView.target);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
            bufferObjects[bufferIdx]); // Binding the index buffer to
                                       // GL_ELEMENT_ARRAY_BUFFER while the
                                       // VAO is bound is enough to tell
                                       // OpenGL we want to use that index
                                       // buffer for that VAO
      }
    }
  }
  glBindVertexArray(0);

  std::clog << "Number of VAOs: " << vertexArrayObjects.size() << std::endl;

  return vertexArrayObjects;
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