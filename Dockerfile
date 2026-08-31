# syntax=docker/dockerfile:1

# ── Stage 1: build ──────────────────────────────────────────────────────────
FROM ubuntu:26.04 AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        make \
        gcc \
        g++ \
        ninja-build \
        clang \
        python3-pip \
        python3-venv \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Conan 2 in an isolated venv so it doesn't conflict with system pip.
RUN python3 -m venv /opt/conan-venv \
    && /opt/conan-venv/bin/pip install --no-cache-dir "conan>=2.0,<3"
ENV PATH="/opt/conan-venv/bin:${PATH}"

# Initialise a default Conan profile with clang.
RUN conan profile detect --force \
    && conan profile show

WORKDIR /src
COPY conanfile.txt ./

# Download and build all Conan dependencies (cached as a separate layer).
# Use the auto-detected profile (gcc on Ubuntu) for dependency builds; clang
# is applied only to our own project in the cmake --preset step below.
RUN mkdir -p build && cd build \
    && conan install .. \
        --build=missing \
        -pr:b=default \
        -s build_type=Release

# Copy the rest of the source and build.
COPY CMakeLists.txt ./
COPY cmake/ cmake/
COPY app/ app/
COPY lib/ lib/

RUN cmake /src \
        -G "Unix Makefiles" \
        -B /src/build/Release \
        -DCMAKE_TOOLCHAIN_FILE=/src/build/Release/generators/conan_toolchain.cmake \
        -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DXMLJSON_WARNINGS_AS_ERRORS=ON \
        -DXMLJSON_BUILD_TESTS=OFF \
    && cmake --build build/Release --target xmljson-service

# ── Stage 2: runtime ────────────────────────────────────────────────────────
FROM ubuntu:26.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system xmljson \
    && useradd --system --gid xmljson --no-create-home xmljson

WORKDIR /app

COPY --from=build /src/build/Release/xmljson-service ./xmljson-service
COPY config/default.json ./config/default.json

RUN chown -R xmljson:xmljson /app

USER xmljson

EXPOSE 8080

ENTRYPOINT ["./xmljson-service"]
CMD ["--config", "config/default.json"]
