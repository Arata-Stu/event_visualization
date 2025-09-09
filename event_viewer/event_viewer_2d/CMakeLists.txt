cmake_minimum_required(VERSION 3.14)
# ★変更: プロジェクト名と使用言語を変更。CUDAは不要になります。
project(EventViewer2D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ★削除: CUDA関連の設定は不要になります。
# set(CMAKE_CUDA_STANDARD 17)
# set(CMAKE_CUDA_STANDARD_REQUIRED ON)
# set(CMAKE_CUDA_ARCHITECTURES "75;86;90")

# ------------------------------------------------------------------
# 1. yaml-cppライブラリをFetchContentで取得 (変更なし)
# ------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG    yaml-cpp-0.7.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_TOOLS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(yaml-cpp)
# ------------------------------------------------------------------

# ライブラリを探す (変更なし)
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS C CXX)

# 実行可能ファイルを作成
# ★変更: 実行可能ファイル名を変更
set(EXECUTABLE_NAME event_viewer_2d)

add_executable(${EXECUTABLE_NAME}
    # ソースファイルリストからcuda_processor.cuを削除
    src/main.cpp
    src/hdf5_loader.cpp
    src/renderer.cpp
    src/image_loader.cpp
    src/camera.cpp      
    src/shader.cpp   
)

# インクルードディレクトリの指定 
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HDF5_INCLUDE_DIRS}
)

# ------------------------------------------------------------------
# 2. shadersディレクトリをビルドディレクトリにコピー 
# ------------------------------------------------------------------
add_custom_command(
    TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/shaders"
    COMMENT "Copying shaders to build directory"
)
# ------------------------------------------------------------------


# 必要なライブラリをリンク 
target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
    ${HDF5_LIBRARIES}
    dl z m
    GLEW::glew
    ${OPENGL_LIBRARIES}
    glfw
    yaml-cpp
)