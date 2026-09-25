# Changelog

All notable changes to PocketMarauder firmware. Format loosely follows
Conventional Commits; each entry: context, change (paths), evidence, impact, rollback.

## Unreleased

### feat: native SSD1306 OLED display backend
- Context: upstream firmware only supports TFT_eSPI color displays; PocketMarauder has a 128x64 mono SSD1306 I2C OLED. Needed a shim exposing the full TFT_eSPI method surface over Adafruit_SSD1306.
- Change:
  - NEW `firmware/esp32_marauder/OledAdapter.h`: `OledDisplay` (wraps `Adafruit_SSD1306`) + `OledButton` (mirrors `TFT_eSPI_Button`). Maps RGB565 -> 1-bit (0x0000 off, else on). Defines TFT_* color/pin/`KEY_*` macros normally from TFT_eSPI. Auto-flushes framebuffer per draw call (ponytail: per-call flush; upgrade to dirty-flag + one `display()` per frame).
  - `Display.h`/`Display.cpp`: compile-select OLED members/methods under `#ifdef POCKET_MARAUDER`; stock TFT path unchanged in `#else`.
  - `drawBootSplash()`: PocketMarauder path draws `pocket_logo.h` (Frenchie) then a "PocketMarauder" + version text card.
  - vendored libs `Adafruit_SSD1306`, `Adafruit_GFX`, `Adafruit_BusIO` into `firmware/esp32_marauder/libraries/`.
- Evidence: pending arduino-cli compile (next step).
- Impact: firmware renders native menu + splash on the OLED without touching other targets.
- Rollback: comment out `#define POCKET_MARAUDER`; adapter is inert without it.

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
