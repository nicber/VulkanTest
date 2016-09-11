cmake_minimum_required(VERSION 3.6)
project(VulkanTest)

list(APPEND CMAKE_MODULE_PATH "${VulkanTest_SOURCE_DIR}/CMake/modules")
find_package(glfw3 3.2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Boost REQUIRED)
find_package(glm REQUIRED)

include_directories(${VULKAN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})
message(${GLM_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

if (WIN32)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
endif ()

file(GLOB HEADERS *.h)
set(SOURCE_FILES main.cpp window.cpp utils.cpp)
add_executable(VulkanTest ${SOURCE_FILES} ${HEADERS})

target_link_libraries(VulkanTest glfw ${VULKAN_LIBRARY})
target_include_directories(VulkanTest PUBLIC "C:/Users/nicol/repos/vkcpp")
set(GLSL_VALIDATOR "glslangValidator")

# stolen from: https://gist.github.com/vlsh/a0d191701cb48f157b05be7f74d79396

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "shaders/*.frag"
        "shaders/*.vert"
        )

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(VulkanTest Shaders)

#add_custom_command(TARGET VulkanTest POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanTest>/shaders/"
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        "${PROJECT_BINARY_DIR}/shaders"
#        "$<TARGET_FILE_DIR:VulkanTest>/shaders"
#        )

add_custom_command(TARGET VulkanTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:glfw>"
        "$<TARGET_FILE_DIR:VulkanTest>")