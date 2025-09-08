#pragma once

#include <glm/glm.hpp>
#include <mq/defs.h>
#include <filesystem>
#include <string>

/* Import GLSL-like types */
using glm::vec2, glm::vec3, glm::vec4;
using glm::mat2, glm::mat3, glm::mat4;

namespace fs = std::filesystem;

struct OpenFileBuffer {
    /* Full path of loaded file (may be relative maybe?) */
    fs::path path = "";
    /* Alloc'd buffer with contents: must free() after use. */
    void *data = nullptr;
    /* Size of file contents. */
    u32 size = 0;
};

/* Load file with C API */
void *openAndReadFile(char const *path, long *size_ptr);

/* Wrapper for native dialog libraries... and pre-reading the file because
   that's how the browser gives it to us in emscripten. There's surely a way to
   not pre-read anything but for ~1 MB files we don't care.

   On emscripten this fills *ofb asynchronously, in-between frames, so this
   must be a static or something safe that won't get lost until then. */
void openFileDialog(OpenFileBuffer *ofb);

/* String describing a memory size, using kiB/MiB suffixes if the size is a
   perfect multiple of 2^10 or 2^20. With shortSuffix, appends k/M. */
std::string memorySizeString(uint size, bool shortSuffix=false);
