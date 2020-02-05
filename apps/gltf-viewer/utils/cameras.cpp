#include "cameras.hpp"
#include "glfw.hpp"

#include <iostream>

// Good reference here to map camera movements to lookAt calls
// http://learnwebgl.brown37.net/07_cameras/camera_movement.html

using namespace glm;

struct ViewFrame
{
  vec3 left;
  vec3 up;
  vec3 front;
  vec3 eye;

  ViewFrame(vec3 l, vec3 u, vec3 f, vec3 e) : left(l), up(u), front(f), eye(e)
  {
  }
};

ViewFrame fromViewToWorldMatrix(const mat4 &viewToWorldMatrix)
{
  return ViewFrame{-vec3(viewToWorldMatrix[0]), vec3(viewToWorldMatrix[1]),
      -vec3(viewToWorldMatrix[2]), vec3(viewToWorldMatrix[3])};
}

bool FirstPersonCameraController::update(float elapsedTime)
{
  if (glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT) &&
      !m_LeftButtonPressed) {
    m_LeftButtonPressed = true;
    glfwGetCursorPos(
        m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
  } else if (!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT) &&
             m_LeftButtonPressed) {
    m_LeftButtonPressed = false;
  }

  const auto cursorDelta = ([&]() {
    if (m_LeftButtonPressed) {
      dvec2 cursorPosition;
      glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
      const auto delta = cursorPosition - m_LastCursorPosition;
      m_LastCursorPosition = cursorPosition;
      return delta;
    }
    return dvec2(0);
  })();

  float truckLeft = 0.f;
  float pedestalUp = 0.f;
  float dollyIn = 0.f;
  float rollRightAngle = 0.f;

  if (glfwGetKey(m_pWindow, GLFW_KEY_W)) {
    dollyIn += m_fSpeed * elapsedTime;
  }

  // Truck left
  if (glfwGetKey(m_pWindow, GLFW_KEY_A)) {
    truckLeft += m_fSpeed * elapsedTime;
  }

  // Pedestal up
  if (glfwGetKey(m_pWindow, GLFW_KEY_UP)) {
    pedestalUp += m_fSpeed * elapsedTime;
  }

  // Dolly out
  if (glfwGetKey(m_pWindow, GLFW_KEY_S)) {
    dollyIn -= m_fSpeed * elapsedTime;
  }

  // Truck right
  if (glfwGetKey(m_pWindow, GLFW_KEY_D)) {
    truckLeft -= m_fSpeed * elapsedTime;
  }

  // Pedestal down
  if (glfwGetKey(m_pWindow, GLFW_KEY_DOWN)) {
    pedestalUp -= m_fSpeed * elapsedTime;
  }

  if (glfwGetKey(m_pWindow, GLFW_KEY_Q)) {
    rollRightAngle -= 0.001f;
  }
  if (glfwGetKey(m_pWindow, GLFW_KEY_E)) {
    rollRightAngle += 0.001f;
  }

  // cursor going right, so minus because we want pan left angle:
  const float panLeftAngle = -0.01f * float(cursorDelta.x);
  const float tiltDownAngle = 0.01f * float(cursorDelta.y);

  const auto hasMoved = truckLeft || pedestalUp || dollyIn || panLeftAngle ||
                        tiltDownAngle || rollRightAngle;
  if (!hasMoved) {
    return false;
  }

  m_camera.moveLocal(truckLeft, pedestalUp, dollyIn);
  m_camera.rotateLocal(rollRightAngle, tiltDownAngle, 0.f);
  m_camera.rotateWorld(panLeftAngle, m_worldUpAxis);

  return true;
}

bool TrackballCameraController::update(float elapsedTime)
{
  if (glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
      !m_MiddleButtonPressed) {
    m_MiddleButtonPressed = true;
    glfwGetCursorPos(
        m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
  } else if (!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
             m_MiddleButtonPressed) {
    m_MiddleButtonPressed = false;
  }

  const auto cursorDelta = ([&]() {
    if (m_MiddleButtonPressed) {
      dvec2 cursorPosition;
      glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
      const auto delta = cursorPosition - m_LastCursorPosition;
      m_LastCursorPosition = cursorPosition;
      return delta;
    }
    return dvec2(0);
  })();

  if (glfwGetKey(m_pWindow, GLFW_KEY_LEFT_SHIFT)) {
    // Pan
    const auto truckLeft = 0.01f * float(cursorDelta.x);
    const auto pedestalUp = 0.01f * float(cursorDelta.y);
    const auto hasMoved = truckLeft || pedestalUp;
    if (!hasMoved) {
      return false;
    }
    m_camera.moveLocal(truckLeft, pedestalUp, 0.f);
    return true;
  }

  if (glfwGetKey(m_pWindow, GLFW_KEY_LEFT_CONTROL)) {
    // Zoom
    auto mouseOffset = 0.01f * float(cursorDelta.x);
    if (mouseOffset == 0.f) {
      return false;
    }

    const auto viewVector = m_camera.center() - m_camera.eye();
    const auto l = glm::length(viewVector);
    const auto front = viewVector / l;
    if (mouseOffset > 0.f) {
      mouseOffset = glm::min(mouseOffset, l - 1e-4f);
    }
    const auto translationVector = mouseOffset * front;
    const auto newEye = m_camera.eye() + translationVector;

    m_camera = Camera(newEye, m_camera.center(), m_worldUpAxis);

    return true;
  }

  const auto longitudeAngle = -0.01f * float(cursorDelta.y);
  const auto latitudeAngle = -0.01f * float(cursorDelta.x);
  const auto hasMoved = longitudeAngle || latitudeAngle;
  if (!hasMoved) {
    return false;
  }

  const auto depthAxis = m_camera.eye() - m_camera.center();
  const auto rotationMatrix =
      glm::rotate(glm::mat4(1), latitudeAngle, m_worldUpAxis);
  const auto rotatedDepthAxis =
      glm::vec3(rotationMatrix * glm::vec4(depthAxis, 0));
  const auto newEye = m_camera.center() + rotatedDepthAxis;
  const auto newDepthAxis = newEye - m_camera.center();
  const auto horizontalAxis = glm::cross(m_worldUpAxis, newDepthAxis);
  const auto newRotationMatrix =
      glm::rotate(glm::mat4(1), longitudeAngle, horizontalAxis);
  const auto newRotatedDepthAxis =
      glm::vec3(newRotationMatrix * glm::vec4(newDepthAxis, 0));
  const auto newNewEye = m_camera.center() + newRotatedDepthAxis;

  m_camera = Camera(newNewEye, m_camera.center(), m_worldUpAxis);

  return true;
}