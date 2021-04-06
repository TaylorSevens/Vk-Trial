project(VkHelloWorld VERSION 0.1.0)

set(src_files
  Cube.cpp
  Sample.cpp
  main.cpp
)

add_executable(${PROJECT_NAME}
 ${src_files}
 )
target_link_libraries(
  ${PROJECT_NAME}
  Common
  DirectXTex
  glfw3
  ${Vulkan_LIBRARIES}
)

file(GLOB SHADER_FILES shaders/*.vert shaders/*.frag)

file(GLOB SHADER_EXTRA_FILES shaders/*.glsl)
set_source_files_properties(${SHADER_EXTRA_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

compile_glsl_to_spirv(SHADER_FILES "shaders" COMPILED_SPIRV_FILES)

source_group("Shaders" FILES ${SHADER_FILES} ${SHADER_EXTRA_FILES})

add_custom_target(
  ${PROJECT_NAME}GenAssets ALL
  DEPENDS ${COMPILED_SPIRV_FILES}
  SOURCES ${SHADER_FILES} ${SHADER_EXTRA_FILES}
)


