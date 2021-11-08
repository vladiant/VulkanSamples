cmake_minimum_required(VERSION 3.10)

project(BasicSampleShaderFile LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(PkgConfig)

pkg_check_modules(GLSLANG REQUIRED glslang)

find_program(GLSLANG_VALIDATOR NAMES glslangValidator)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)

add_custom_command(COMMENT "Compiling compute shader"
                   OUTPUT simple_shader.comp.spv
                   COMMAND ${GLSLANG_VALIDATOR} -V -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/simple_shader.comp.spv
                           ${PROJECT_SOURCE_DIR}/shaders/simple_shader.comp
                   MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/shaders/simple_shader.comp
                   DEPENDS ${PROJECT_SOURCE_DIR}/shaders/simple_shader.comp ${GLSLANG_VALIDATOR})
add_custom_target(ComputeShader ALL DEPENDS simple_shader.comp.spv)

add_executable(
    ${PROJECT_NAME}
    main.cpp
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        Vulkan::Vulkan
)

add_dependencies(${PROJECT_NAME} ComputeShader)
