# Ubuntu 26.04 LTS base
FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. Base Toolchain & Rendering Libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc g++ clang \
    gdb make ninja-build \
    git curl ca-certificates tar xz-utils unzip openssh-client \
    # OpenGL & Windowing System Headers
    libgl1-mesa-dev libegl1-mesa-dev libgbm-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libwayland-dev wayland-protocols \
    python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# 2. Pin CMake Version (Kitware Direct Install)
ARG CMAKE_VERSION=3.30.2
RUN curl -sSL https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz \
    | tar -xz -C /usr/local --strip-components=1

# 3. Pin Vulkan SDK (LunarG Direct Archive Install)
ARG VULKAN_SDK_VERSION=1.4.357.1

RUN mkdir -p /opt/vulkan && \
    curl -sSL -o /tmp/vulkansdk.tar.xz "https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz" && \
    tar -xJf /tmp/vulkansdk.tar.xz -C /opt/vulkan --strip-components=1 && \
    rm /tmp/vulkansdk.tar.xz

# Set path environment variables directly to /opt/vulkan
ENV VULKAN_SDK=/opt/vulkan/x86_64
ENV PATH=$VULKAN_SDK/bin:$PATH
ENV LD_LIBRARY_PATH=$VULKAN_SDK/lib:$LD_LIBRARY_PATH
ENV VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d

# 4. Prebuilt SwiftShader from GitHub Release URL
# Pass SWIFTSHADER_URL during docker build or set the default below (.tar.gz, .zip, or .tar.xz)
ARG SWIFTSHADER_URL="https://github.com/dantros/swiftshader_prebuilts/archive/refs/tags/swiftshader-d26a3e66-linux-x86_64.tar.gz"

RUN mkdir -p /opt/swiftshader && \
    curl -sSL "${SWIFTSHADER_URL}" | tar -xz -C /opt/swiftshader

# Environment variables pointing to the prebuilt binaries
ENV SWIFTSHADER_PATH=/opt/swiftshader
ENV LD_LIBRARY_PATH=/opt/swiftshader:$LD_LIBRARY_PATH

# 5. Non-Root Development User setup
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Delete default 'ubuntu' user/group (freeing up UID/GID 1000)
RUN touch /var/mail/ubuntu && chown ubuntu /var/mail/ubuntu && userdel -r ubuntu || true

# Pass -s /bin/bash to set bash as the default shell
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m -s /bin/bash $USERNAME \
    && apt-get update \
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME \
    && rm -rf /var/lib/apt/lists/*

USER $USERNAME
WORKDIR /workspace

CMD ["/bin/bash"]