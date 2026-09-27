cmake_minimum_required(VERSION 3.16)
project(Astra VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Все зависимости должны линковаться в libastra_engine.so
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glm glad)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glad_SOURCE_DIR}/include
)
target_link_libraries(imgui PUBLIC glfw glad)

# Движок как shared-библиотека: игры линкуются с ней, а не дублируют бинарник
add_library(AstraEngine SHARED
    src/core/Application.cpp
    src/core/Window.cpp
    src/core/Renderer.cpp
    src/core/Camera.cpp
    src/core/GUI.cpp
    src/core/GameUI.cpp
    src/core/Input.cpp
    src/core/Scene.cpp
    src/core/SceneSerializer.cpp
    src/core/ProjectManager.cpp
    src/core/Scripting.cpp
    src/core/Audio.cpp
    src/utils/ConsoleLog.cpp
    src/utils/AssetIO.cpp
    src/ecs/SceneManager.cpp
    src/ecs/Physics.cpp
    src/utils/Shader.cpp
    src/utils/Framebuffer.cpp
    src/utils/Texture.cpp
    src/vendor/stb_image_impl.cpp
    src/vendor/miniaudio_impl.cpp
)

set_target_properties(AstraEngine PROPERTIES OUTPUT_NAME "astra_engine")

target_include_directories(AstraEngine PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${glad_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${stb_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
)

# Пути для компиляции игровых скриптов (g++ -shared на Play)
set(SCRIPT_INCLUDES
    "-I${CMAKE_SOURCE_DIR}/src -I${glm_SOURCE_DIR} -I${glfw_SOURCE_DIR}/include -I${imgui_SOURCE_DIR} -I${stb_SOURCE_DIR} -I${glad_SOURCE_DIR}/include")

target_compile_definitions(AstraEngine PUBLIC
    TEMPLATE_DIR="${CMAKE_SOURCE_DIR}/templates/default_project"
    SCRIPT_INCLUDES="${SCRIPT_INCLUDES}"
    ASTRA_SRC_DIR="${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(AstraEngine PUBLIC
    glfw
    glm
    glad
    imgui
    ${CMAKE_DL_LIBS}
)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE AstraEngine)

# -rdynamic: скрипты (.so) резолвят символы движка через dlopen
target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
