project(VkHelloWorld VERSION 0.1.0)

set(src_files
  Cube.cpp
)

add_executable(${PROJECT_NAME}
 ${src_files}
 )
target_link_libraries(
  ${PROJECT_NAME}
  Common
  DirectXTex
  ${glfw_LIBRARIES}
  ${Vulkan_LIBRARIES}
)

file(GLOB shader_files shaders/*.vert shaders/*.frag
  shaders/*.rgen shaders/*.rchit shaders/*.rint shaders/*.rmiss)

file(GLOB shader_extra_files shaders/*.glsl)
set_source_files_properties(${shader_extra_files} PROPERTIES HEADER_FILE_ONLY TRUE)

# Shader compilation
foreach(shader ${shader_files})
	#message("SHADER: ${shader}")
	get_filename_component(file_name ${shader} NAME)
	get_filename_component(full_path ${shader} ABSOLUTE)
	set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${TRIAL_OUTDIR_SUFFIX}/shaders)
	set(output_file ${output_dir}/${file_name}.spv)
	set(compiled_shaders ${compiled_shaders} ${output_file})
	set(compiled_shaders ${compiled_shaders} PARENT_SCOPE)
	set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
	if (WIN32)
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${Vulkan_GLSLANG_VALIDATOR} -V ${full_path} -o ${output_file}
            DEPENDS ${full_path}
        )
    else()
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND mkdir --parents ${output_dir} && ${Vulkan_GLSLANG_VALIDATOR} -V ${full_path} -o ${output_file}
            DEPENDS ${full_path}
        )
    endif()
endforeach()

source_group("Shaders" FILES ${shader_files} ${shader_extra_files})

add_custom_target(
  ${PROJECT_NAME}GenAssets ALL
  DEPENDS ${compiled_shaders}
  SOURCES ${shader_files} ${shader_extra_files}
)


