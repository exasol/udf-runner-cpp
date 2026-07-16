FROM {{udfclient_deps}}

COPY --from={{build_deps}} /usr /usr
COPY --from={{build_deps}} /lib /lib
COPY --from={{build_deps}} /bin /bin
COPY --from={{build_deps}} /opt /opt
COPY --from={{build_deps}} /etc /etc
COPY --from={{build_deps}} /build_info /build_info
RUN true

RUN exaslpm export-variables --out-file /env
RUN ldconfig

RUN mkdir /exaudfclient /exaudf
COPY exaudfclient/ /exaudfclient/

WORKDIR /exaudfclient/
RUN ["/bin/bash", "-c", "source /env && bash build.sh --lockfile_mode=off --config no-tty --config fast-binary --define benchmark=true --define bash=true //:udf_runner_cpp_v1_gen //:udf_runner_cpp_v1_static_gen"]

RUN cp -r -L bazel-bin/* /exaudf
RUN mv /exaudf/udf_runner_cpp_v1 /exaudf/exaudfclient
RUN mv /exaudf/udf_runner_cpp_v1_static /exaudf/exaudfclient_static

WORKDIR /exaudfclient/base
RUN ./test_udfclient.sh /exaudf/exaudfclient
