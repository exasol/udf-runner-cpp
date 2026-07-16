FROM exasol/script-language-container:exaslpm-1.3.1-ubuntu-24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p /build_info/packages

COPY build_deps_packages.yml /build_info/packages/build_deps_packages.yml
RUN exaslpm install --package-file /build_info/packages/build_deps_packages.yml --build-step build_deps

RUN locale-gen en_US.UTF-8 && \
    update-locale LC_ALL=en_US.UTF-8 && \
    ldconfig
