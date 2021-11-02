cmake_minimum_required(VERSION 3.10)

project(BasicSampleC LANGUAGES C)

find_package(Vulkan REQUIRED)

add_executable(
    ${PROJECT_NAME}
    main.c
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        Vulkan::Vulkan
)

