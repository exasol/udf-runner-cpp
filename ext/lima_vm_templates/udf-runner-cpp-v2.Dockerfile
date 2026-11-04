FROM ubuntu:24.04

ARG BAZELISK_VERSION=1.27.0

ENV DEBIAN_FRONTEND=noninteractive \
    USE_BAZEL_VERSION=8.8.2 \
    JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 \
    EXASOL_TELEMETRY_DISABLE=yes

RUN apt-get update && apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        curl \
        git \
        gnupg \
        libpcre3-dev \
        libprotobuf-dev \
        libzmq3-dev \
        openjdk-17-jdk \
        protobuf-compiler \
        python3.12-dev \
        python3-pip \
        python3-venv \
        lsb-release \
        software-properties-common \
        unzip \
        wget \
        && rm -rf /var/lib/apt/lists/*

RUN wget --quiet --output-document=- https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor --output /etc/apt/trusted.gpg.d/llvm.gpg \
    && add-apt-repository -y 'deb https://apt.llvm.org/noble/ llvm-toolchain-noble-22 main' \
    && apt-get update \
    && apt-get install --no-install-recommends -y \
           clang-22 \
           clang-format-22 \
           clang-tidy-22 \
    && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh

RUN curl --fail --location --output /usr/local/bin/bazel \
        "https://github.com/bazelbuild/bazelisk/releases/download/v${BAZELISK_VERSION}/bazelisk-linux-amd64" \
    && chmod 0755 /usr/local/bin/bazel \
    && ln -s /usr/local/bin/bazel /usr/local/bin/bazelisk \
    && clang-format-22 --version \
    && clang-tidy-22 --version \
    && bazel --version

WORKDIR /workspace/udf-runner-cpp/v2

ENTRYPOINT ["/usr/local/bin/bazel"]
