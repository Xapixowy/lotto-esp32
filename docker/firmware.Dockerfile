FROM python:3.12-slim
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl git \
    && rm -rf /var/lib/apt/lists/*
RUN case "$TARGETARCH" in arm64) archive=Linux_ARM64 ;; amd64) archive=Linux_64bit ;; *) exit 1 ;; esac \
    && curl -fsSL "https://github.com/arduino/arduino-cli/releases/download/v1.5.1/arduino-cli_1.5.1_${archive}.tar.gz" | tar -xz -C /usr/local/bin arduino-cli
RUN arduino-cli config init \
    && arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json \
    && arduino-cli core update-index \
    && arduino-cli core install esp32:esp32@3.3.0 \
    && arduino-cli lib install 'ArduinoJson@7.4.2' 'Adafruit ILI9341' 'Adafruit ST7735 and ST7789 Library' 'U8g2_for_Adafruit_GFX' 'XPT2046_Touchscreen'
WORKDIR /workspace
RUN apt-get update && apt-get install -y --no-install-recommends g++ && rm -rf /var/lib/apt/lists/*
CMD ["arduino-cli", "compile", "--fqbn", "esp32:esp32:esp32:PartitionScheme=huge_app", "--build-path", "/tmp/lotto-build", "firmware/LottoDisplay"]
