# Changelog

All notable changes to PocketMarauder firmware. Format loosely follows
Conventional Commits; each entry: context, change (paths), evidence, impact, rollback.

## Unreleased

### feat: add POCKET_MARAUDER board target
- Context: custom board (ESP32-WROOM-32D, SSD1306 128x64 I2C OLED, 4 buttons) needs its own target in the vendored ESP32Marauder firmware.
- Change: `firmware/esp32_marauder/configs.h`
  - board flag `POCKET_MARAUDER` (active) in BOARD TARGETS
  - `HARDWARE_NAME "PocketMarauder"`
  - feature block: `HAS_BT`, `HAS_BUTTONS`, `HAS_SCREEN`, `HAS_MINI_SCREEN` (no SD/GPS/battery/temp)
  - button block: L=IO32, C=IO33, U=IO35, D=IO25, R=-1; all `_PULL true` (external 10k)
  - display block: OLED_SDA=21, OLED_SCL=22, OLED_ADDR=0x3C, 128x64, `MENU_FONT NULL`
- Evidence: pinmap verified from `PCB/EAGLE/espmarauder.sch` net analysis.
- Impact: selects the custom board at compile time; other targets untouched (commented).
- Rollback: comment out `#define POCKET_MARAUDER` at configs.h line ~48.
