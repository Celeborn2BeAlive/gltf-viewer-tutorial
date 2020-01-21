#pragma once

#include "gl_debug_output.hpp"
#include "glfw.hpp"
#include <glm/glm.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <stdexcept>

// Class responsible for initializing GLFW, creating a window, initializing
// OpenGL function pointers with GLAD library and initializing ImGUI
class GLFWHandle
{
public:
  GLFWHandle(int width, int height, const char *title, bool visible = true)
  {
    if (!glfwInit()) {
      std::cerr << "Unable to init GLFW.\n";
      throw std::runtime_error("Unable to init GLFW.\n");
    }

    if (!visible) {
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_pWindow =
        glfwCreateWindow(int(width), int(height), title, nullptr, nullptr);
    if (!m_pWindow) {
      std::cerr << "Unable to open window.\n";
      glfwTerminate();
      throw std::runtime_error("Unable to open window.\n");
    }

    glfwMakeContextCurrent(m_pWindow);

    glfwSwapInterval(0); // No VSync

    if (!gladLoadGL()) {
      std::cerr << "Unable to init OpenGL.\n";
      throw std::runtime_error("Unable to init OpenGL.\n");
    }

    initGLDebugOutput();

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(m_pWindow, true);
    const char *glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);
  }

  ~GLFWHandle()
  {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
  }

  // Non-copyable class:
  GLFWHandle(const GLFWHandle &) = delete;
  GLFWHandle &operator=(const GLFWHandle &) = delete;

  bool shouldClose() const { return glfwWindowShouldClose(m_pWindow); }

  glm::ivec2 framebufferSize() const
  {
    int displayWidth, displayHeight;
    glfwGetFramebufferSize(m_pWindow, &displayWidth, &displayHeight);
    return glm::ivec2(displayWidth, displayHeight);
  }

  void swapBuffers() const { glfwSwapBuffers(m_pWindow); }

  GLFWwindow *window() { return m_pWindow; }

private:
  GLFWwindow *m_pWindow = nullptr;
};

inline void imguiNewFrame()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

inline void imguiRenderFrame()
{
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

inline void printGLVersion()
{
  GLint glVersion[2];
  glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
  glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

  std::clog << "OpenGL Version " << glVersion[0] << "." << glVersion[1]
            << std::endl;
}