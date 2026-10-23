#!/usr/bin/env bash
# Compile the QT Py bridge in a throwaway container (arduino-cli + Adafruit SAMD core). Output: tools/bridge/build/*.uf2
# Flash: double-tap the QT Py reset button, then copy the .uf2 onto the QTPY_BOOT drive.
set -euo pipefail
cd "$(dirname "$0")"
docker run --rm -v "$PWD:/b" -w /b ubuntu:24.04 bash -c '
  set -e; apt-get update -qq && apt-get install -y -qq curl ca-certificates >/dev/null
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh >/dev/null
  arduino-cli config init >/dev/null
  arduino-cli config add board_manager.additional_urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
  arduino-cli core update-index >/dev/null && arduino-cli core install adafruit:samd >/dev/null
  arduino-cli compile --fqbn adafruit:samd:adafruit_qtpy_m0 --output-dir build qtpy_bridge
  ls build'
