#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;

// Camera defined by an eye position, a center position and an up vector
class Camera
{
public:
  Camera() = default;

  Camera(glm::vec3 e, glm::vec3 c, glm::vec3 u) : m_eye(e), m_center(c), m_up(u)
  {
    const auto front = m_center - m_eye;
    const auto left = cross(m_up, front);
    assert(left != glm::vec3(0));
    m_up = normalize(cross(front, left));
  };

  glm::mat4 getViewMatrix() const { return glm::lookAt(m_eye, m_center, m_up); }

  // Move the camera along its left axis.
  void truckLeft(float offset)
  {
    const auto front = m_center - m_eye;
    const auto left = normalize(cross(m_up, front));
    const auto translationVector = offset * left;
    m_eye += translationVector;
    m_center += translationVector;
  }

  void pedestalUp(float offset)
  {
    const auto translationVector = offset * m_up;
    m_eye += translationVector;
    m_center += translationVector;
  }

  void dollyIn(float offset)
  {
    const auto front = normalize(m_center - m_eye);
    const auto translationVector = offset * front;
    m_eye += translationVector;
    m_center += translationVector;
  }

  void moveLocal(float truckLeftOffset, float pedestalUpOffset, float dollyIn)
  {
    const auto front = normalize(m_center - m_eye);
    const auto left = normalize(cross(m_up, front));
    const auto translationVector =
        truckLeftOffset * left + pedestalUpOffset * m_up + dollyIn * front;
    m_eye += translationVector;
    m_center += translationVector;
  }

  void rollRight(float radians)
  {
    const auto front = m_center - m_eye;
    const auto rollMatrix = glm::rotate(glm::mat4(1), radians, front);

    m_up = glm::vec3(rollMatrix * glm::vec4(m_up, 0.f));
  }

  void tiltDown(float radians)
  {
    const auto front = m_center - m_eye;
    const auto left = cross(m_up, front);
    const auto tiltMatrix = glm::rotate(glm::mat4(1), radians, left);

    const auto newFront = glm::vec3(tiltMatrix * glm::vec4(front, 0.f));
    m_center = m_eye + newFront;
    m_up = glm::vec3(tiltMatrix * glm::vec4(m_up, 0.f));
  }

  void panLeft(float radians)
  {
    const auto front = m_center - m_eye;
    const auto panMatrix = glm::rotate(glm::mat4(1), radians, m_up);

    const auto newFront = glm::vec3(panMatrix * glm::vec4(front, 0.f));
    m_center = m_eye + newFront;
  }

  // All angles in radians
  void rotateLocal(float rollRight, float tiltDown, float panLeft)
  {
    const auto front = m_center - m_eye;
    const auto rollMatrix = glm::rotate(glm::mat4(1), rollRight, front);

    m_up = glm::vec3(rollMatrix * glm::vec4(m_up, 0.f));

    const auto left = cross(m_up, front);

    const auto tiltMatrix = glm::rotate(glm::mat4(1), tiltDown, left);

    const auto newFront = glm::vec3(tiltMatrix * glm::vec4(front, 0.f));
    m_center = m_eye + newFront;

    m_up = glm::vec3(tiltMatrix * glm::vec4(m_up, 0.f));

    const auto panMatrix = glm::rotate(glm::mat4(1), panLeft, m_up);

    const auto newNewFront = glm::vec3(panMatrix * glm::vec4(newFront, 0.f));
    m_center = m_eye + newNewFront;
  }

  // Rotate around a world axis but keep the same position
  void rotateWorld(float radians, const glm::vec3 &axis)
  {
    const auto rotationMatrix = glm::rotate(glm::mat4(1), radians, axis);

    const auto front = m_center - m_eye;
    const auto newFront = glm::vec3(rotationMatrix * glm::vec4(front, 0));
    m_center = m_eye + newFront;
    m_up = glm::vec3(rotationMatrix * glm::vec4(m_up, 0));
  }

  const glm::vec3 eye() const { return m_eye; }

  const glm::vec3 center() const { return m_center; }

  const glm::vec3 up() const { return m_up; }

  const glm::vec3 front(bool normalize = true) const
  {
    const auto f = m_center - m_eye;
    return normalize ? glm::normalize(f) : f;
  }

  const glm::vec3 left(bool normalize = true) const
  {
    const auto f = front(false);
    const auto l = glm::cross(m_up, f);
    return normalize ? glm::normalize(l) : l;
  }

private:
  glm::vec3 m_eye;
  glm::vec3 m_center;
  glm::vec3 m_up;
};

class FirstPersonCameraController
{
public:
  FirstPersonCameraController(GLFWwindow *window, float speed = 1.f,
      const glm::vec3 &worldUpAxis = glm::vec3(0, 1, 0)) :
      m_pWindow(window),
      m_fSpeed(speed),
      m_worldUpAxis(worldUpAxis),
      m_camera{glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)}
  {
  }

  // Controller attributes, if put in a GUI, should be adapted
  void setSpeed(float speed) { m_fSpeed = speed; }

  float getSpeed() const { return m_fSpeed; }

  void increaseSpeed(float delta)
  {
    m_fSpeed += delta;
    m_fSpeed = glm::max(m_fSpeed, 0.f);
  }

  const glm::vec3 &getWorldUpAxis() const { return m_worldUpAxis; }

  void setWorldUpAxis(const glm::vec3 &worldUpAxis)
  {
    m_worldUpAxis = worldUpAxis;
  }

  // Update the view matrix based on input events and elapsed time
  // Return true if the view matrix has been modified
  bool update(float elapsedTime);

  // Get the view matrix
  const Camera &getCamera() const { return m_camera; }

  void setCamera(const Camera &camera) { m_camera = camera; }

private:
  GLFWwindow *m_pWindow = nullptr;
  float m_fSpeed = 0.f;
  glm::vec3 m_worldUpAxis;

  // Input event state
  bool m_LeftButtonPressed = false;
  glm::dvec2 m_LastCursorPosition;

  // Current camera
  Camera m_camera;
};

// todo Blender like camera
class TrackballCameraController
{
public:
  TrackballCameraController(GLFWwindow *window, float speed = 1.f,
      const glm::vec3 &worldUpAxis = glm::vec3(0, 1, 0)) :
      m_pWindow(window),
      m_fSpeed(speed),
      m_worldUpAxis(worldUpAxis),
      m_camera{glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)}
  {
  }

  // Controller attributes, if put in a GUI, should be adapted
  void setSpeed(float speed) { m_fSpeed = speed; }

  float getSpeed() const { return m_fSpeed; }

  void increaseSpeed(float delta)
  {
    m_fSpeed += delta;
    m_fSpeed = glm::max(m_fSpeed, 0.f);
  }

  const glm::vec3 &getWorldUpAxis() const { return m_worldUpAxis; }

  void setWorldUpAxis(const glm::vec3 &worldUpAxis)
  {
    m_worldUpAxis = worldUpAxis;
  }

  // Update the view matrix based on input events and elapsed time
  // Return true if the view matrix has been modified
  bool update(float elapsedTime);

  // Get the view matrix
  const Camera &getCamera() const { return m_camera; }

  void setCamera(const Camera &camera) { m_camera = camera; }

private:
  GLFWwindow *m_pWindow = nullptr;
  float m_fSpeed = 0.f;
  glm::vec3 m_worldUpAxis;

  // Input event state
  bool m_MiddleButtonPressed = false;
  glm::dvec2 m_LastCursorPosition;

  // Current camera
  Camera m_camera;
};