# Vendored Prusa-Firmware-Buddy subset (printer side of the MMU protocol)

Source: https://github.com/prusa3d/Prusa-Firmware-Buddy tag `v6.10.1` (GPL-3.0, see `LICENSE.md`). Unmodified copies,
same relative paths as upstream, used only to build `tools/vprinter` (host test harness). Includes Buddy's own vendored
copy of the MMU protocol codec (`lib/Prusa-Firmware-MMU/...`) so the virtual printer does not share code with our fork.
