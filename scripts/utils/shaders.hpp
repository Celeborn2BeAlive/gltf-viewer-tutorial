#pragma once

#include "filesystem.hpp"
#include <fstream>
#include <glad/glad.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>


class GLShader
{
  GLuint m_GLId;
  typedef std::unique_ptr<char[]> CharBuffer;

public:
  GLShader(GLenum type) : m_GLId(glCreateShader(type)) {}

  ~GLShader() { glDeleteShader(m_GLId); }

  GLShader(const GLShader &) = delete;

  GLShader &operator=(const GLShader &) = delete;

  GLShader(GLShader &&rvalue) : m_GLId(rvalue.m_GLId) { rvalue.m_GLId = 0; }

  GLShader &operator=(GLShader &&rvalue)
  {
    this->~GLShader();
    m_GLId = rvalue.m_GLId;
    rvalue.m_GLId = 0;
    return *this;
  }

  GLuint glId() const { return m_GLId; }

  void setSource(const GLchar *src) { glShaderSource(m_GLId, 1, &src, 0); }

  void setSource(const std::string &src) { setSource(src.c_str()); }

  bool compile()
  {
    glCompileShader(m_GLId);
    return getCompileStatus();
  }

  bool getCompileStatus() const
  {
    GLint status;
    glGetShaderiv(m_GLId, GL_COMPILE_STATUS, &status);
    return status == GL_TRUE;
  }

  std::string getInfoLog() const
  {
    GLint logLength;
    glGetShaderiv(m_GLId, GL_INFO_LOG_LENGTH, &logLength);

    CharBuffer buffer(new char[logLength]);
    glGetShaderInfoLog(m_GLId, logLength, 0, buffer.get());

    return std::string(buffer.get());
  }
};

inline std::string loadShaderSource(const fs::path &filepath)
{
  std::ifstream input(filepath.string());
  if (!input) {
    std::stringstream ss;
    ss << "Unable to open file " << filepath;
    throw std::runtime_error(ss.str());
  }

  std::stringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

template <typename StringType>
GLShader compileShader(GLenum type, StringType &&src)
{
  GLShader shader(type);
  shader.setSource(std::forward<StringType>(src));
  if (!shader.compile()) {
    std::cerr << shader.getInfoLog() << std::endl;
    throw std::runtime_error(shader.getInfoLog());
  }
  return shader;
}

// Load and compile a shader according to the following naming convention:
// *.vs.glsl -> vertex shader
// *.fs.glsl -> fragment shader
// *.gs.glsl -> geometry shader
// *.cs.glsl -> compute shader
inline GLShader loadShader(const fs::path &shaderPath)
{
  static auto extToShaderType =
      std::unordered_map<std::string, std::pair<GLenum, std::string>>(
          {{".vs", {GL_VERTEX_SHADER, "vertex"}},
              {".fs", {GL_FRAGMENT_SHADER, "fragment"}},
              {".gs", {GL_GEOMETRY_SHADER, "geometry"}},
              {".cs", {GL_COMPUTE_SHADER, "compute"}}});

  const auto ext = shaderPath.stem().extension();
  const auto it = extToShaderType.find(ext.string());
  if (it == end(extToShaderType)) {
    std::cerr << "Unrecognized shader extension " << ext << std::endl;
    throw std::runtime_error("Unrecognized shader extension " + ext.string());
  }

  std::clog << "Compiling " << (*it).second.second << " shader " << shaderPath
            << "\n";

  GLShader shader{(*it).second.first};
  shader.setSource(loadShaderSource(shaderPath));
  shader.compile();
  if (!shader.getCompileStatus()) {
    std::cerr << "Shader compilation error:" << shader.getInfoLog()
              << std::endl;
    throw std::runtime_error("Shader compilation error:" + shader.getInfoLog());
  }
  return shader;
}

class GLProgram
{
  GLuint m_GLId;
  typedef std::unique_ptr<char[]> CharBuffer;

public:
  GLProgram() : m_GLId(glCreateProgram()) {}

  ~GLProgram() { glDeleteProgram(m_GLId); }

  GLProgram(const GLProgram &) = delete;

  GLProgram &operator=(const GLProgram &) = delete;

  GLProgram(GLProgram &&rvalue) : m_GLId(rvalue.m_GLId) { rvalue.m_GLId = 0; }

  GLProgram &operator=(GLProgram &&rvalue)
  {
    this->~GLProgram();
    m_GLId = rvalue.m_GLId;
    rvalue.m_GLId = 0;
    return *this;
  }

  GLuint glId() const { return m_GLId; }

  void attachShader(const GLShader &shader)
  {
    glAttachShader(m_GLId, shader.glId());
  }

  bool link()
  {
    glLinkProgram(m_GLId);
    return getLinkStatus();
  }

  bool getLinkStatus() const
  {
    GLint linkStatus;
    glGetProgramiv(m_GLId, GL_LINK_STATUS, &linkStatus);
    return linkStatus == GL_TRUE;
  }

  std::string getInfoLog() const
  {
    GLint logLength;
    glGetProgramiv(m_GLId, GL_INFO_LOG_LENGTH, &logLength);

    CharBuffer buffer(new char[logLength]);
    glGetProgramInfoLog(m_GLId, logLength, 0, buffer.get());

    return std::string(buffer.get());
  }

  void use() const { glUseProgram(m_GLId); }

  GLint getUniformLocation(const GLchar *name) const
  {
    GLint location = glGetUniformLocation(m_GLId, name);
    return location;
  }

  GLint getAttribLocation(const GLchar *name) const
  {
    GLint location = glGetAttribLocation(m_GLId, name);
    return location;
  }

  void bindAttribLocation(GLuint index, const GLchar *name) const
  {
    glBindAttribLocation(m_GLId, index, name);
  }
};

inline GLProgram buildProgram(std::initializer_list<GLShader> shaders)
{
  GLProgram program;
  for (const auto &shader : shaders) {
    program.attachShader(shader);
  }

  if (!program.link()) {
    std::cerr << program.getInfoLog() << std::endl;
    throw std::runtime_error(program.getInfoLog());
  }

  return program;
}

template <typename VSrc, typename FSrc>
GLProgram buildProgram(VSrc &&vsrc, FSrc &&fsrc)
{
  GLShader vs = compileShader(GL_VERTEX_SHADER, std::forward<VSrc>(vsrc));
  GLShader fs = compileShader(GL_FRAGMENT_SHADER, std::forward<FSrc>(fsrc));

  return buildProgram({std::move(vs), std::move(fs)});
}

template <typename VSrc, typename GSrc, typename FSrc>
GLProgram buildProgram(VSrc &&vsrc, GSrc &&gsrc, FSrc &&fsrc)
{
  GLShader vs = compileShader(GL_VERTEX_SHADER, std::forward<VSrc>(vsrc));
  GLShader gs = compileShader(GL_GEOMETRY_SHADER, std::forward<GSrc>(gsrc));
  GLShader fs = compileShader(GL_FRAGMENT_SHADER, std::forward<FSrc>(fsrc));

  return buildProgram({std::move(vs), std::move(gs), std::move(fs)});
}

template <typename CSrc> GLProgram buildComputeProgram(CSrc &&src)
{
  GLShader cs = compileShader(GL_COMPUTE_SHADER, std::forward<CSrc>(src));
  return buildProgram({std::move(cs)});
  ;
}

inline GLProgram compileProgram(std::vector<fs::path> shaderPaths)
{
  GLProgram program;
  for (const auto &path : shaderPaths) {
    auto shader = loadShader(path);
    program.attachShader(shader);
  }
  program.link();
  if (!program.getLinkStatus()) {
    std::cerr << "Program link error:" << program.getInfoLog() << std::endl;
    throw std::runtime_error("Program link error:" + program.getInfoLog());
  }
  return program;
}
