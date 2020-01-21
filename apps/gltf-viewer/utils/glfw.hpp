#pragma once

// This header is responsible for including glad and glfw headers in the correct
// order. Glad should be included first, thats why we disable clang format here
// to avoid reordering includes (we could also put a blank line between the two
// includes)
// http://www.glfw.org/docs/latest/quick.html

// clang-format off

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// clang-format on