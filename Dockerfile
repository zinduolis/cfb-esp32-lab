FROM espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805

ARG TARGETARCH
ARG ARDUINO_CLI_VERSION=1.4.1
ARG ARDUINO_CLI_SHA256_AMD64=683cf2a6b8953e3d632e7e4512c36667839d2073349c4b6d312e4c67592359bd
ARG ARDUINO_CLI_SHA256_ARM64=93159a5e27af6dab03bd3b5a441c86092d83c0422a5c17d0afc2ac21aee83612

ENV ARDUINO_BOARD_MANAGER_URL=https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
ENV ARDUINO_DIRECTORIES_DATA=/opt/arduino-cli/data
ENV ARDUINO_DIRECTORIES_DOWNLOADS=/opt/arduino-cli/staging
ENV ARDUINO_DIRECTORIES_USER=/opt/arduino-cli/user
ENV ARDUINO_FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc

RUN case "${TARGETARCH}" in \
      "amd64") archive="arduino-cli_${ARDUINO_CLI_VERSION}_Linux_64bit.tar.gz" \
               sha256="${ARDUINO_CLI_SHA256_AMD64}" ;; \
      "arm64") archive="arduino-cli_${ARDUINO_CLI_VERSION}_Linux_ARM64.tar.gz" \
               sha256="${ARDUINO_CLI_SHA256_ARM64}" ;; \
      *) echo "Unsupported architecture: ${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && curl -fsSL "https://downloads.arduino.cc/arduino-cli/${archive}" -o /tmp/arduino-cli.tar.gz \
    && echo "${sha256}  /tmp/arduino-cli.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/arduino-cli.tar.gz -C /usr/local/bin arduino-cli \
    && rm /tmp/arduino-cli.tar.gz

# 0777 required: docker-run passes -u $(id -u):$(id -g) so runtime UID is unknown at build time
RUN mkdir -p "${ARDUINO_DIRECTORIES_DATA}" "${ARDUINO_DIRECTORIES_DOWNLOADS}" "${ARDUINO_DIRECTORIES_USER}" \
    && chmod -R 0777 /opt/arduino-cli \
    && arduino-cli core update-index --additional-urls "${ARDUINO_BOARD_MANAGER_URL}" \
    && arduino-cli core install esp32:esp32 --additional-urls "${ARDUINO_BOARD_MANAGER_URL}" \
    && arduino-cli lib install "U8g2"

WORKDIR /workspace

CMD ["bash"]
