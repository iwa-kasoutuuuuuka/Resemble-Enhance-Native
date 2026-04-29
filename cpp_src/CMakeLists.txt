cmake_minimum_required(VERSION 3.10)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(ResembleDebugV60)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch dependencies (standard way)
include(FetchContent)

FetchContent_Declare(glfw URL https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90.1)
FetchContent_Declare(implot GIT_REPOSITORY https://github.com/epezent/implot.git GIT_TAG v0.16)

# Force timestamps to avoid warnings
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(glfw imgui implot)

# Paths
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMPLOT_DIR ${implot_SOURCE_DIR})

include_directories(
    include
    ${glfw_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    # LibTorch & ONNX paths (restored from previous knowledge)
    "f:/antigravity/miniconda/Lib/site-packages/torch/include"
    "f:/antigravity/miniconda/Lib/site-packages/torch/include/torch/csrc/api/include"
    "f:/antigravity/packages/Microsoft.ML.OnnxRuntime.1.20.1/build/native/include"
    "f:/antigravity/ffmpeg_dev/ffmpeg-master-latest-win64-gpl-shared/include"
)

# AI libraries
link_directories(
    "f:/antigravity/miniconda/Lib/site-packages/torch/lib"
    "f:/antigravity/packages/Microsoft.ML.OnnxRuntime.1.20.1/runtimes/win-x64/native"
    "f:/antigravity/ffmpeg_dev/ffmpeg-master-latest-win64-gpl-shared/lib"
)

file(GLOB SOURCES 
    "src/*.cpp"
    "${IMGUI_DIR}/imgui*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMPLOT_DIR}/implot*.cpp"
    "resources.rc"
)

add_executable(ResembleDebugV60 ${SOURCES})

target_link_libraries(ResembleDebugV60 PRIVATE 
    glfw
    opengl32
    winmm
    ole32
    uuid
    # AI/Audio Libs
    torch torch_cpu c10
    onnxruntime
    avformat avcodec avutil swresample
)

# Set runtime library
if(MSVC)
    add_compile_options("/utf-8")
endif()
set_property(TARGET ResembleDebugV60 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
