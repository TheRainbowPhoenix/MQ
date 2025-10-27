cmake_minimum_required(VERSION 3.15)
project(MQ VERSION 0.1 LANGUAGES CXX C ASM)

list(APPEND CMAKE_MODULE_PATH "$ENV{AZUR_PATH_${AZUR_PLATFORM}}/lib/cmake")
find_package(Azur 0.2 REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(AZUR_PLATFORM STREQUAL gint)
  message(FATAL_ERROR "gint not supported, this only runs on computer!")
endif()

# Folder for auto-generated files
set(AUTOGEN "${CMAKE_CURRENT_BINARY_DIR}/autogen")

# Configuration modes and options
# TODO: This should only apply to Linux builds.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # In Debug mode, optimizations are enabled unless -DMQ_DISABLE_OPTIMIZATIONS
  if(MQ_DISABLE_OPTIMIZATIONS)
    set(MQ_OPT_CC -O0)
  else()
    set(MQ_OPT_CC -O3 --profiling -g)
  endif()
  set(MQ_OPT_LD)

  # Profiling
  if(MQ_PROFILING_TRACY)
    list(APPEND MQ_OPT_CC -DTRACY_ENABLE)
  elseif(MQ_PROFILING_GPROF)
    list(APPEND MQ_OPT_CC -pg)
    list(APPEND MQ_OPT_LD -pg)
  endif()
else()
  # In Release mode, optimizations are enabled and LTO is too
  set(MQ_OPT_CC -O3 -flto=auto)
  set(MQ_OPT_LD -flto=auto)
endif()

set(LIB_SOURCES
  core/controller.c
  core/cpu.c
  core/defs.c
  core/hooks.c
  core/insn.c
  core/machine.c
  core/memory.c
  core/mq.c
  "${AUTOGEN}/assets.c"

  interfaces/display.c
  interfaces/keyboard.c
  interfaces/timer.c

  modules/cmod.c
  modules/dma.c
  modules/keysc.c
  modules/intc.c
  modules/mmu.c
  modules/r61524.c
  modules/t6k11.c
  modules/tmu.c
  modules/cpg.c
  modules/rtc.c

  system/casiowin.c
  system/casiowin-getkey.c
  system/casiowin-render-fx.c
  system/casiowin-rtc.c
  system/heap.c)

add_custom_command(OUTPUT "${AUTOGEN}/sh-isa.inc"
  COMMAND mkdir -p "${AUTOGEN}"
  COMMAND python tools/gen-isa.py --table core/sh-isa.def
                 "${AUTOGEN}/sh-isa.inc"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating decoder for sh-isa"
  DEPENDS tools/gen-isa.py core/sh-isa.def)
# Annoyingly, I couldn't get the dependency to be auto-detected. Anything that
# uses a custom target is out of the question because that always rebuilds.
set_source_files_properties(core/insn.c PROPERTIES
  OBJECT_DEPENDS "${AUTOGEN}/sh-isa.inc")

# Assets converted via a Python script equivalent to xxd -i into a header.
# Having a single file is not super elegant but it simplifies the dependencies
# a lot (it's pulled by source files and the header comes with it, so it's
# always up-to-date with no manual dependency declarations)
set(EMBEDDED_ASSETS
  system/fx-font.bin
  system/fx-font-mini.bin)
add_custom_command(
  OUTPUT "${AUTOGEN}/assets.c" "${AUTOGEN}/assets.h"
  COMMAND mkdir -p "${AUTOGEN}"
  COMMAND python tools/gen-assets.py
                 -c "${AUTOGEN}/assets.c" -h "${AUTOGEN}/assets.h"
                 ${EMBEDDED_ASSETS}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Embeddeding assets"
  DEPENDS tools/gen-assets.py ${EMBEDDED_ASSETS})

#=== Main GUI executable ======================================================#

set(GUI_SOURCES
  gui/display-window.cc
  gui/gui.cc
  gui/imgui-util.cc
  gui/main.cc
  gui/stb.cc
  gui/util.cc
  gui/watch.cc)

set(ASSETS
  gui/assets/DejaVuSans.subset.ttf
  gui/assets/DejaVuSansMono.subset.ttf
  gui/assets/DejaVuSans-Bold.subset.ttf
  gui/assets/icons.png
  gui/glsl/vs_tiles.glsl
  gui/glsl/fs_tiles.glsl)

azur_embed_assets(
  OUTPUT "${AUTOGEN}/gui-assets.cc"
  NAME "mqgui"
  RELATIVE_TO "gui"
  ${ASSETS})

add_executable(mq ${LIB_SOURCES} ${GUI_SOURCES} "${AUTOGEN}/gui-assets.cc")
target_compile_options(mq PRIVATE
  ${MQ_OPT_CC} -g -Wall -Wextra -Wno-pointer-arith -pthread)
target_include_directories(mq PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tracy/public")
target_link_options(mq PRIVATE ${MQ_OPT_LD} -pthread)

# TODO: Use Tracy's CMake integration just to make things a bit cleaner.
if(MQ_PROFILING_TRACY)
  target_sources(mq PRIVATE 3rdparty/tracy/public/TracyClient.cpp)
  target_compile_options(mq PRIVATE -pthread)
  target_link_options(mq PRIVATE -pthread)
  target_link_libraries(mq PRIVATE -ldl)
endif()

if(AZUR_PLATFORM STREQUAL linux)
  # target_compile_options(mq PRIVATE -fsanitize=address)
  # target_link_options(mq PRIVATE -fsanitize=address)
  target_link_libraries(mq PRIVATE Azur::Azur Azur::GLM Azur::ImGui)
endif()

if(AZUR_PLATFORM STREQUAL emscripten)
  target_link_libraries(mq PRIVATE Azur::Azur Azur::GLM Azur::ImGui)
  # Export _malloc and a few other functions even if unused, because the file
  # dialog calls them from JS code that's not evaluated at compile time
  target_link_options(mq PRIVATE
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','ccall']"
    "-sEXPORTED_RUNTIME_METHODS=['HEAPU8']")

  # Embed assets to load them without filesystem access
  foreach(ASSET IN LISTS ASSETS)
    target_link_options(mq PRIVATE
      "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/${ASSET}@${ASSET}")
  endforeach()

  # Add assets to link dependencies
  set(ASSETS_ABSOLUTE)
  foreach(ASSET IN LISTS ASSETS)
    list(APPEND ASSETS_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET}")
  endforeach()
  set_target_properties(mq PROPERTIES LINK_DEPENDS "${ASSETS_ABSOLUTE}")

  azur_generate_html_template("${CMAKE_CURRENT_BINARY_DIR}/index.html" "mq.js")
endif()

#=== Headless executable ======================================================#

if(NOT AZUR_PLATFORM STREQUAL emscripten)
  set(HEADLESS_SOURCES
    headless/main.c
    ${LIB_SOURCES})

  if(MQ_PROFILING_TRACY)
    list(APPEND HEADLESS_SOURCES
      3rdparty/tracy/public/TracyClient.cpp)
  endif()

  add_executable(mq-headless ${HEADLESS_SOURCES})
  target_compile_options(mq-headless PRIVATE
    ${MQ_OPT_CC} -g -Wall -Wextra -Wno-pointer-arith)
  target_include_directories(mq-headless PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tracy/public")
  target_link_options(mq-headless PRIVATE ${MQ_OPT_LD})
endif()
