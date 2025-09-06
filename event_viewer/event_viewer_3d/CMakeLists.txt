cmake_minimum_required(VERSION 3.14) # FetchContentのためバージョンを少し上げます
project(EventViewer3D LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES "75;86;90")

# ------------------------------------------------------------------
# 1. yaml-cppライブラリをFetchContentで取得
# ------------------------------------------------------------------
# これにより、システムにyaml-cppがインストールされていなくても
# CMakeが自動的にダウンロードしてビルドに含めてくれます。
include(FetchContent)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG    yaml-cpp-0.8.0 # 特定のバージョンを指定
)
FetchContent_MakeAvailable(yaml-cpp)
# ------------------------------------------------------------------

# ライブラリを探す
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS C CXX)

# 実行可能ファイルを作成
set(EXECUTABLE_NAME event_viewer_3d)
add_executable(${EXECUTABLE_NAME}
    src/main.cpp
    src/hdf5_loader.cpp
    src/cuda_processor.cu
    src/renderer.cpp
    src/image_loader.cpp
    src/camera.cpp      
    src/shader.cpp   
)

# 各ターゲットに必要なインクルードディレクトリを指定 (モダンな方法)
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include # プロジェクトのincludeディレクトリ
    ${HDF5_INCLUDE_DIRS}
    # yaml-cppはFetchContent経由で自動的に追加されます
)

# ------------------------------------------------------------------
# 2. shadersディレクトリをビルドディレクトリにコピー (より堅牢な方法)
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
    yaml-cpp # 
)