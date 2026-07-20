FROM {{build_deps}}
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p /build_info/packages
COPY udfclient_deps_packages.yml /build_info/packages/udfclient_deps_packages.yml
RUN exaslpm install --package-file /build_info/packages/udfclient_deps_packages.yml --build-step udfclient_deps

RUN usermod -l exasolution ubuntu
RUN groupmod -n exasolution ubuntu
RUN groupadd --gid 500 exausers
RUN useradd --uid 500 --gid 500 --no-create-home --shell /usr/sbin/nologin --comment "First Last,RoomNumber,WorkPhone,HomePhone" exadefusr

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
