#!/usr/bin/env bash
# Compile the QT Py bridge in a throwaway container (arduino-cli + Adafruit SAMD core). Output: tools/bridge/build/*.uf2
# Flash: double-tap the QT Py reset button, then copy the .uf2 onto the QTPY_BOOT drive.
set -euo pipefail
cd "$(dirname "$0")"
docker run --rm -v "$PWD:/b" -w /b ubuntu:24.04 bash -c '
  set -e; apt-get update -qq && apt-get install -y -qq curl ca-certificates python3 >/dev/null
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh >/dev/null
  arduino-cli config init >/dev/null
  arduino-cli config add board_manager.additional_urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
  arduino-cli core update-index >/dev/null && arduino-cli core install adafruit:samd >/dev/null
  arduino-cli compile --fqbn adafruit:samd:adafruit_qtpy_m0 --output-dir build qtpy_bridge
  # .bin -> .uf2 for drag-and-drop flashing (SAMD21 app starts after the 8 KiB UF2 bootloader)
  curl -fsSL -o /tmp/uf2conv.py https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2conv.py
  curl -fsSL -o /tmp/uf2families.json https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json
  python3 /tmp/uf2conv.py -c -b 0x2000 -f SAMD21 -o build/qtpy_bridge.uf2 build/qtpy_bridge.ino.bin
  ls build'
