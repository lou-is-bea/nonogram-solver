# Reference environment of the project (docs/TESTING.md §5 and §8), used by the CI and for
# reproducing it locally:
#   docker build -t nonogram-dev tools/docker
#   docker run --rm -v "$PWD:/src" -w /src nonogram-dev ./scripts/check.sh
#
# Everything is frozen: the base image by digest, the Ubuntu archive by snapshot date and every
# tool by exact version. Updating a version is a reviewed change to this file, and the pins
# must match the packages installed by scripts/setup-wsl.sh.

# ubuntu:24.04
FROM ubuntu@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac

ARG UBUNTU_SNAPSHOT=20261020T000000Z
ENV DEBIAN_FRONTEND=noninteractive

# ca-certificates comes first: the snapshot service is only reachable over HTTPS.
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && apt-get update --snapshot "${UBUNTU_SNAPSHOT}" \
    && apt-get install -y --no-install-recommends --snapshot "${UBUNTU_SNAPSHOT}" \
        build-essential=12.10ubuntu1 \
        gcc-14=14.2.0-4ubuntu2~24.04.1 \
        libstdc++-14-dev=14.2.0-4ubuntu2~24.04.1 \
        clang-20=1:20.1.2-0ubuntu1~24.04.3 \
        clang-19=1:19.1.1-1ubuntu1~24.04.2 \
        clang-format-20=1:20.1.2-0ubuntu1~24.04.3 \
        clang-tidy-20=1:20.1.2-0ubuntu1~24.04.3 \
        libclang-rt-20-dev=1:20.1.2-0ubuntu1~24.04.3 \
        llvm-20=1:20.1.2-0ubuntu1~24.04.3 \
        cmake=3.28.3-1build7 \
        ninja-build=1.11.1-2 \
        pkg-config=1.8.1-2build1 \
        libcriterion-dev=2.4.1-2build2 \
        valgrind=1:3.22.0-0ubuntu3 \
        cppcheck=2.13.0-2ubuntu3 \
        gcovr=7.0-1 \
        python3=3.12.3-0ubuntu2.1 \
        git=1:2.43.0-1ubuntu7.4 \
    && rm -rf /var/lib/apt/lists/*

# The CI checks the repository out with a different owner than the container user; check.sh
# relies on `git ls-files`.
RUN git config --system --add safe.directory '*'

WORKDIR /src
