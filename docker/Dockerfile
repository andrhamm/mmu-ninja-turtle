# Ninja Turtle dev/CI image: host build + tests (GCC), ARM cross toolchain, Python, socat.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git python3 \
        gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
        socat ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Bind-mounted repo is owned by the host user; let git (build number) read it.
RUN git config --system --add safe.directory "*"
WORKDIR /work
