FROM {{base_test_deps}}

COPY --from={{build_run}} /usr /usr
RUN true
COPY --from={{build_run}} /lib /lib
RUN true
COPY --from={{build_run}} /bin /bin
RUN true
COPY --from={{build_run}} /opt /opt
RUN true
COPY --from={{build_run}} /etc /etc
RUN true
COPY --from={{build_run}} /build_info /build_info
RUN true
COPY --from={{build_run}} /exaudf /exaudf
RUN true

RUN ldconfig
RUN mkdir -p /conf /buckets
