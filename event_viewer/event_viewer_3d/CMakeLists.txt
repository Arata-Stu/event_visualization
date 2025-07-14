cmake_minimum_required(VERSION 3.10)
project(EventViewer LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES "75;86;90")

# OpenGL, GLEW, GLFW ライブラリを探す
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# HDF5ライブラリを探す
find_package(HDF5 REQUIRED COMPONENTS C CXX)

# includeディレクトリをインクルードパスに追加
include_directories(include)

# 実行可能ファイルを作成
add_executable(event_viewer
    src/main.cpp
    src/hdf5_loader.cpp
    src/cuda_processor.cu
    src/renderer.cpp
    src/image_loader.cpp
)

# shadersディレクトリをビルドディレクトリにコピー
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})

# HDF5のインクルードディレクトリをターゲットに追加
target_include_directories(event_viewer PRIVATE ${HDF5_INCLUDE_DIRS})

# 必要なライブラリをリンク
target_link_libraries(event_viewer
    PRIVATE
    ${HDF5_LIBRARIES}
    dl z m
    GLEW::glew
    ${OPENGL_LIBRARIES}
    glfw
)