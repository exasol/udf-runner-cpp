FROM {{build_run}}

COPY language_definitions.json /exaudf/language_definitions.json

RUN mkdir -p /build_info/actual_installed_packages/release && \
    exaslpm list-all-installed-packages --out-file /build_info/actual_installed_packages/release/packages.yml
