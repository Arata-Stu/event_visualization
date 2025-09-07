# Base image: Use the official NVIDIA CUDA 12.2.2 image for Ubuntu 22.04
FROM nvidia/cuda:12.2.2-devel-ubuntu22.04

# Set environment variables to prevent interactive prompts during installation
ENV TZ=Asia/Tokyo
ARG DEBIAN_FRONTEND=noninteractive

# 1. Update system and install application dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    wget \
    vim \
    nano \
    tmux \
    terminator \
    x11-apps \
    git \
    cmake \
    build-essential \
    ca-certificates \
    libhdf5-dev \
    libblosc-dev \
    libglew-dev \
    libglfw3-dev \
    libx11-dev \
    libglm-dev \
    xorg-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# HDF5 Blosc plugin build and install
WORKDIR /tmp
RUN git clone https://github.com/Blosc/hdf5-blosc.git && \
    cd hdf5-blosc && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    mkdir -p /usr/local/hdf5/lib/plugin && \
    cp libH5Zblosc.so /usr/local/hdf5/lib/plugin/ && \
    rm -rf /tmp/hdf5-blosc

# Set HDF5 plugin path environment variable
ENV HDF5_PLUGIN_PATH=/usr/local/hdf5/lib/plugin/

# Create working directory and clone the event_visualization repository
WORKDIR /workspace
RUN git clone https://github.com/Arata-Stu/event_visualization.git

# Set the final working directory for when the container starts
WORKDIR /workspace/event_visualization/event_viewer/event_viewer_3d

# Set the default command to start a bash shell
CMD ["bash"]
