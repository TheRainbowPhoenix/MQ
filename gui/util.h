#pragma once

#include <glm/glm.hpp>
#include <mq/defs.h>
#include <string>

/* Import GLSL-like types */
using glm::vec2, glm::vec3, glm::vec4;
using glm::mat2, glm::mat3, glm::mat4;

/* Portable File Dialog wrappers (header takes too long to compile) */
std::string openFileDialog();
