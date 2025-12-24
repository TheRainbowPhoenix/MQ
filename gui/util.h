#pragma once

#include <glm/glm.hpp>
#include <mq/defs.h>
#include <mq/interfaces/display.h>
#include <azur/defs.h>
#include <filesystem>
#include <functional>

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

/* Generate random frames on the given display. */
bool generateMonoFrame(mqDisplay *display);
bool generateRGBFrame(mqDisplay *display);

/* Replace all occurrences of a substring with another substring. */
std::string replaceSubstring(
   std::string const &text, std::string const &pat, std::string const &repl);

/* Return strftime string from current time. */
std::string strftimeCurrentTime(char const *fmt);

/* Generate a PNG of the given display after applying a scale factor. Returns
   an error code; description is obtained with screenshotPNG_strerror. */
int screenshotPNG(mqDisplay *display, int scale, std::string const &path);
std::string screenshotPNG_strerror(int err);

/* simple path utilities */
bool fileIsDirectory(std::string const &path);
bool fileExists(std::string const &path);

/* Utility class to name an output file with a substitution pattern, determine
   if it already exists and, if requested, automatically make it unique with a
   numerical suffix. With lazy stat() calls and lazy substitutions (because
   %DATE% will be a field and calling it all the time is). */
class OutputPathPattern
{
public:
   /* Each substitution maps a pattern to a pair (value, description). */
   using SubstitutionMap = map<string, std::pair<string, string>>;
   /* Function generating the substitution on-demand. */
   using Substitution = std::function<SubstitutionMap()>;

   /* Get or set the pattern. */
   std::string pattern() const { return m_pattern; }
   void setPattern(std::string const &pattern);

   /* Set whether we can overwrite an existing file or if we rename the output
      file with a -N suffix to make it unique. */
   bool overwrite() const { return m_overwrite; }
   void setOverwrite(bool overwrite);

   /* Set the substitution (as a lazy function), or reset it. Note: this only
      sets dirty when changing the function that generates the SubstitutionMap,
      and doesn't automatically re-resolve the path whenever the map changes.
      To force resolution, call resolve(true). */
   void setSubstitution(Substitution const &s) { m_subst = s; m_dirty = true; }
   void resetSubstitution() { setSubstitution(noSubstitution); }
   /* Get latest substitution map. */
   SubstitutionMap const &getSubstitutionMap() { return m_substMap; }

   /* Resolve the path. This is idempotent and runs automatically when changing
      the pattern, overwrite setting, or substitution function. This can be
      called with force=true to force re-evaluating the substitution. */
   void resolve(bool force=false) const;

   /* Resolved path: substituted pattern plus its potential unique suffix. */
   std::string const &resolvedPath() const { resolve(); return m_resolvedPath; }
   /* Whether the resolved path already exists (possible when overwriting). */
   bool resolvedPathExists() const { resolve(); return m_resolvedPathExists; }

private:
   static bool fileExists(std::string path);
   static std::string makePathWithSuffix(std::string path, int uniqueID);
   static SubstitutionMap noSubstitution() { return {}; }

   /* Input settings: pattern, substitution, and overwrite */
   std::string m_pattern = "";
   Substitution m_subst = noSubstitution;
   bool m_overwrite = false;
   /* Input has changed and we haven't resolved it yet */
   mutable bool m_dirty = true;
   /* Current substitution, resolved path and whether it exists */
   mutable SubstitutionMap m_substMap;
   mutable std::string m_resolvedPath;
   mutable bool m_resolvedPathExists;
};
