FROM {{release}}
ENV DEBIAN_FRONTEND=noninteractive

COPY scripts /scripts

ENTRYPOINT ["/scripts/security_scan/run.sh"]
