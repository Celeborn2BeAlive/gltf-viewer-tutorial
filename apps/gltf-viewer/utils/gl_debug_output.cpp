#include "gl_debug_output.hpp"
#include <algorithm>
#include <array>
#include <glad/glad.h>
#include <imgui.h>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <vector>

static const std::unordered_map<GLenum, const char *> sourceEnumToString = {
    {GL_DEBUG_SOURCE_API, "API"},
    {GL_DEBUG_SOURCE_WINDOW_SYSTEM, "WINDOW_SYSTEM"},
    {GL_DEBUG_SOURCE_SHADER_COMPILER, "SHADER_COMPILER"},
    {GL_DEBUG_SOURCE_THIRD_PARTY, "THIRD_PARTY"},
    {GL_DEBUG_SOURCE_APPLICATION, "APPLICATION"},
    {GL_DEBUG_SOURCE_OTHER, "OTHER"}};

static const std::unordered_map<GLenum, const char *> typeEnumToString = {
    {GL_DEBUG_TYPE_ERROR, "ERROR"},
    {GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DEPRECATED_BEHAVIOR"},
    {GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "UNDEFINED_BEHAVIOR"},
    {GL_DEBUG_TYPE_PORTABILITY, "PORTABILITY"},
    {GL_DEBUG_TYPE_PERFORMANCE, "PERFORMANCE"}, {GL_DEBUG_TYPE_OTHER, "OTHER"}};

static const std::unordered_map<GLenum, const char *> severityEnumToString = {
    {GL_DEBUG_SEVERITY_HIGH, "HIGH"}, {GL_DEBUG_SEVERITY_MEDIUM, "MEDIUM"},
    {GL_DEBUG_SEVERITY_LOW, "LOW"},
    {GL_DEBUG_SEVERITY_NOTIFICATION, "NOTIFICATION"}};

// List of message type to ignore for GL Debug Output
static const std::vector<std::tuple<GLenum, GLenum, GLenum>> ignoreList = {
    std::make_tuple(GL_DONT_CARE, GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION) // Ignore all notifications
};

static std::array<std::tuple<const char *, bool, GLenum>, 6> sourceSelector = {
    std::make_tuple("API", true, GL_DEBUG_SOURCE_API),
    std::make_tuple("WINDOW_SYSTEM", true, GL_DEBUG_SOURCE_WINDOW_SYSTEM),
    std::make_tuple("SHADER_COMPILER", true, GL_DEBUG_SOURCE_SHADER_COMPILER),
    std::make_tuple("THIRD_PARTY", true, GL_DEBUG_SOURCE_THIRD_PARTY),
    std::make_tuple("APPLICATION", true, GL_DEBUG_SOURCE_APPLICATION),
    std::make_tuple("OTHER", true, GL_DEBUG_SOURCE_OTHER),
};

static std::array<std::tuple<const char *, bool, GLenum>, 6> typeSelector = {
    std::make_tuple("ERROR", true, GL_DEBUG_TYPE_ERROR),
    std::make_tuple(
        "DEPRECATED_BEHAVIOR", true, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR),
    std::make_tuple(
        "UNDEFINED_BEHAVIOR", true, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR),
    std::make_tuple("PORTABILITY", true, GL_DEBUG_TYPE_PORTABILITY),
    std::make_tuple("PERFORMANCE", true, GL_DEBUG_TYPE_PERFORMANCE),
    std::make_tuple("OTHER", true, GL_DEBUG_TYPE_OTHER),
};

static std::array<std::tuple<const char *, bool, GLenum>, 4> severitySelector =
    {std::make_tuple("HIGH", true, GL_DEBUG_SEVERITY_HIGH),
        std::make_tuple("MEDIUM", true, GL_DEBUG_SEVERITY_MEDIUM),
        std::make_tuple("LOW", true, GL_DEBUG_SEVERITY_LOW),
        std::make_tuple("NOTIFICATION", false, GL_DEBUG_SEVERITY_NOTIFICATION)};

void logGLDebugInfo(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, GLvoid *userParam);

void initGLDebugOutput()
{
  glDebugMessageCallback((GLDEBUGPROCARB)logGLDebugInfo, nullptr);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

  for (const auto &tuple : ignoreList) {
    glDebugMessageControl(std::get<0>(tuple), std::get<1>(tuple),
        std::get<2>(tuple), 0, nullptr, GL_FALSE);
  }
}

void logGLDebugInfo(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, GLvoid *userParam)
{
  const auto findStr = [](GLenum value, const auto &map) {
    const auto it = map.find(value);
    if (it == end(map)) {
      return "UNDEFINED";
    }
    return (*it).second;
  };

  const auto sourceStr = findStr(source, sourceEnumToString);
  const auto typeStr = findStr(type, typeEnumToString);
  const auto severityStr = findStr(severity, severityEnumToString);

  std::clog << "OpenGL: " << message << " [source=" << sourceStr
            << " type=" << typeStr << " severity=" << severityStr
            << " id=" << id << "]\n\n";
}
