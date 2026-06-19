FROM mcr.microsoft.com/devcontainers/cpp:dev-ubuntu-24.04 AS base

RUN apt update && export DEBIAN_FRONTEND=noninteractive && apt -y install clangd-19 clang-tidy-19 openmpi-bin openmpi-doc libopenmpi-dev
RUN update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-19 100
RUN update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-19 100

RUN apt update && export DEBIAN_FRONTEND=noninteractive && apt -y install python3-sphinx python3-breathe doxygen python3-sphinx-rtd-theme

# Always build YGM tests in devcontainer
ENV YGM_BUILD_TESTS=ON