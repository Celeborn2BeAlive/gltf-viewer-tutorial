#pragma once

#include <glm/glm.hpp>
#include <tiny_gltf.h>

glm::mat4 getLocalToWorldMatrix(
    const tinygltf::Node &node, const glm::mat4 &parentMatrix);

void computeSceneBounds(
    const tinygltf::Model &model, glm::vec3 &bboxMin, glm::vec3 &bboxMax);