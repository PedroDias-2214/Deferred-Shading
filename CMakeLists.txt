cmake_minimum_required(VERSION 3.20)
project(DeferredVulkan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Caminhos base
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "Variável VULKAN_SDK não definida. Instale o Vulkan SDK.")
endif()
set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty)
set(GLFW_DIR ${THIRDPARTY_DIR}/GLFW)

# Compilador de shaders GLSL -> SPIR-V (vem no Vulkan SDK)
find_program(GLSLC glslc HINTS ${VULKAN_SDK_PATH}/Bin ${VULKAN_SDK_PATH}/bin REQUIRED)

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${SHADER_SRC_DIR}/spv)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
    ${SHADER_SRC_DIR}/fullscreen.vert
    ${SHADER_SRC_DIR}/geometry.vert
    ${SHADER_SRC_DIR}/geometry.frag
    ${SHADER_SRC_DIR}/lighting.frag
)

set(SPV_FILES)
foreach(SRC ${SHADER_SOURCES})
    get_filename_component(NAME ${SRC} NAME)
    set(SPV ${SHADER_OUT_DIR}/${NAME}.spv)
    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${GLSLC} ${SRC} -o ${SPV}
        DEPENDS ${SRC}
        COMMENT "Compilando ${NAME} para SPIR-V"
    )
    list(APPEND SPV_FILES ${SPV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_FILES})

# ---------------------------------------------------------------------
# Executável
# ---------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/Instance.cpp
    src/Device.cpp
    src/Swapchain.cpp
    src/Shaders.cpp
    src/GBuffer.cpp
    src/RenderPass.cpp
    src/Buffer.cpp
    src/Descriptors.cpp
    src/LightingPipeline.cpp
    src/Renderer.cpp
    src/DefaultMaterial.cpp
    src/Mesh.cpp
    src/GeometryPipeline.cpp
)

add_executable(DeferredVulkan ${APP_SOURCES})
add_dependencies(DeferredVulkan Shaders)

target_include_directories(DeferredVulkan SYSTEM PRIVATE
    ${VULKAN_SDK_PATH}/Include
    ${GLFW_DIR}/include
)

target_link_directories(DeferredVulkan PRIVATE
    ${VULKAN_SDK_PATH}/Lib
    ${GLFW_DIR}/lib
)

target_link_libraries(DeferredVulkan PRIVATE vulkan-1 glfw3)

# Copia os .spv para junto do executável, onde o programa vai carregá-los
add_custom_command(TARGET DeferredVulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUT_DIR}
        $<TARGET_FILE_DIR:DeferredVulkan>/spv
)