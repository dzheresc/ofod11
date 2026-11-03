# ofod11 development image: Ubuntu 24.04 toolchain for C++20 + Python 3.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG VCPKG_TAG=2026.10.16

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++-13 gcc-13 \
        clang-18 clang-tidy-18 clang-format-18 lld-18 libclang-rt-18-dev \
        cmake ninja-build ccache pkg-config \
        git curl ca-certificates zip unzip tar \
        gdb valgrind heaptrack linux-tools-generic \
        python3 python3-dev python3-venv python3-pip \
        kcachegrind massif-visualizer \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 \
    && ln -sf "$(ls -d /usr/lib/linux-tools/*/perf | head -n1)" /usr/local/bin/perf

# FlameGraph scripts for tools/profile/perf.sh.
ARG FLAMEGRAPH_COMMIT=41fee1f99f9276008b7cd112fca19dc3ea84ac32
RUN git init -q /opt/FlameGraph \
    && git -C /opt/FlameGraph fetch -q --depth 1 https://github.com/brendangregg/FlameGraph.git ${FLAMEGRAPH_COMMIT} \
    && git -C /opt/FlameGraph checkout -q FETCH_HEAD
ENV PATH=/opt/FlameGraph:${PATH}

# uv: fast Python env and package manager
COPY --from=ghcr.io/astral-sh/uv:0.9.5 /uv /uvx /usr/local/bin/
RUN UV_TOOL_DIR=/opt/uv-tools UV_TOOL_BIN_DIR=/usr/local/bin uv tool install ruff==0.6.9

# vcpkg (manifest mode; binary cache lives in a named volume)
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_DEFAULT_BINARY_CACHE=/root/.cache/vcpkg \
    VCPKG_FORCE_SYSTEM_BINARIES=1
RUN git clone --depth 1 --branch ${VCPKG_TAG} https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics \
    && ln -s ${VCPKG_ROOT}/vcpkg /usr/local/bin/vcpkg \
    && mkdir -p ${VCPKG_DEFAULT_BINARY_CACHE}

ENV CCACHE_DIR=/root/.cache/ccache \
    UV_CACHE_DIR=/root/.cache/uv \
    UV_LINK_MODE=copy \
    CMAKE_GENERATOR=Ninja

# Mark the bind-mounted Windows checkout as safe for git inside the container.
RUN git config --system --add safe.directory '*' \
    && git config --system core.filemode false

WORKDIR /develop/ofod11
CMD ["bash"]
