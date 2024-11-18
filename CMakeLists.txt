cmake_minimum_required(VERSION 3.15)
project(MQ VERSION 0.1 LANGUAGES CXX C ASM)

list(APPEND CMAKE_MODULE_PATH "$ENV{AZUR_PATH_${AZUR_PLATFORM}}/lib/cmake")
find_package(Azur 0.1 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(AZUR_PLATFORM STREQUAL gint)
  message(FATAL_ERROR "gint not supported, this only runs on computer!")
endif()

set(SOURCES
  src/main.cc
  src/programs.cpp)
set(ASSETS)

if(AZUR_PLATFORM STREQUAL emscripten)
  configure_file(backend/emscripten/index.html index.html)
  list(APPEND ASSETS
    assets/DejaVuSans.ttf
    glsl/vs_bg.glsl
    glsl/fs_bg.glsl
  )
endif()

add_executable(mq ${SOURCES} ${ASSETS})
target_compile_options(mq PRIVATE -O0 -g)
target_link_options(mq PRIVATE)

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
