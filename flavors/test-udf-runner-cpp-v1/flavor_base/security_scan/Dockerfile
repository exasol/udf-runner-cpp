FROM {{release}}
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p /build_info/packages
COPY security_scan_packages.yml /build_info/packages/security_scan_packages.yml
COPY scripts /scripts

ENTRYPOINT ["/scripts/security_scan/run.sh"]
