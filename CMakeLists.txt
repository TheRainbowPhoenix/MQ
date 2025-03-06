cmake_minimum_required(VERSION 3.15)
project(MQ VERSION 0.1 LANGUAGES CXX C ASM)

list(APPEND CMAKE_MODULE_PATH "$ENV{AZUR_PATH_${AZUR_PLATFORM}}/lib/cmake")
find_package(Azur 0.1 REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(AZUR_PLATFORM STREQUAL gint)
  message(FATAL_ERROR "gint not supported, this only runs on computer!")
endif()

set(LIB_SOURCES
  core/cpu.c
  core/defs.c
  core/hooks.c
  core/insn.c
  core/machine.c
  core/memory.c
  core/mq.c

  interfaces/display.c
  interfaces/keyboard.c
  interfaces/timer.c

  modules/cmod.c
  modules/dma.c
  modules/keysc.c
  modules/intc.c
  modules/mmu.c
  modules/r61524.c

  system/casiowin.c
  system/heap.c)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/autogen/sh-isa.inc"
  COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/autogen"
  COMMAND python tools/gen-isa.py core/sh-isa.def
                 "${CMAKE_CURRENT_BINARY_DIR}/autogen/sh-isa.inc"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating decoder for sh-isa"
  DEPENDS tools/gen-isa.py core/sh-isa.def)
# Annoyingly, I couldn't get the dependency to be auto-detected. Anything that
# uses a custom target is out of the question because that always rebuilds.
set_source_files_properties(core/insn.c PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/autogen/sh-isa.inc")

#=== Main GUI executable ======================================================#

set(GUI_SOURCES
  gui/display-window.cc
  gui/imgui-util.cc
  gui/main.cc
  gui/texture.cc
  gui/util.cc
  gui/windows.cc)

set(ASSETS)

if(AZUR_PLATFORM STREQUAL emscripten)
  configure_file(gui/emscripten/index.html index.html)
  list(APPEND ASSETS
    gui/assets/DejaVuSans.ttf
    gui/assets/DejaVuSansMono.ttf
    gui/glsl/vs_tiles.glsl
    gui/glsl/fs_tiles.glsl
  )
endif()

add_executable(mq ${LIB_SOURCES} ${GUI_SOURCES} ${ASSETS})
target_compile_options(mq PRIVATE
  -O3 -flto=auto -g -Wall -Wextra -Wno-pointer-arith -fsanitize=address)
target_include_directories(mq PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_link_options(mq PRIVATE -flto=auto -fsanitize=address)

if(AZUR_PLATFORM STREQUAL linux)
  target_link_libraries(mq Azur::Azur Azur::GLM Azur::ImGui)
endif()

if(AZUR_PLATFORM STREQUAL emscripten)
  target_link_libraries(mq Azur::Azur Azur::GLM Azur::ImGui)
  # Works around a bug on my machine where emcc fails to minify the source code
  # due to a stupid API-version-mismatch TypeError
  target_compile_options(mq PRIVATE -g2)
  target_link_options(mq PRIVATE -g2)

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

  # Generate an accompanying HTML file
  set_target_properties(mq PROPERTIES SUFFIX ".html")
endif()

#=== Headless executable ======================================================#

if(NOT AZUR_PLATFORM STREQUAL emscripten)
  add_executable(mq-headless ${LIB_SOURCES} headless/main.c)
  target_compile_options(mq-headless PRIVATE
    -O3 -flto=auto -g -Wall -Wextra -Wno-pointer-arith)
  target_include_directories(mq-headless PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_options(mq-headless PRIVATE -flto=auto)
endif()
