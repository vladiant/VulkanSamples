cmake_minimum_required(VERSION 3.10)

project(ExternalCompilation LANGUAGES CXX)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(PkgConfig)

pkg_check_modules(GLSLANG REQUIRED glslang)

find_program(GLSLANG_VALIDATOR NAMES glslangValidator)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(COMMENT "Compiling ${FILE_NAME}"
                       OUTPUT ${FILE_NAME}
                       COMMAND ${GLSLANG_VALIDATOR} ${GLSL} -V -o ${SPIRV}                                
                       MAIN_DEPENDENCY ${GLSL}
                       DEPENDS ${GLSL} ${GLSLANG_VALIDATOR})
    list(APPEND SPIRV_TARGETS ${FILE_NAME})
endforeach(GLSL)

add_custom_target(Shaders ALL DEPENDS ${SPIRV_TARGETS})

add_executable(
    ${PROJECT_NAME}
    lve_window.hpp
    lve_window.cpp
    first_app.hpp
    first_app.cpp
    lve_pipeline.hpp
    lve_pipeline.cpp
    lve_device.hpp
    lve_device.cpp
    lve_swap_chain.hpp
    lve_swap_chain.cpp
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
        glfw
        Vulkan::Vulkan
)

add_dependencies(${PROJECT_NAME} Shaders)
