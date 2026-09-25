# Changelog

All notable changes to PocketMarauder firmware. Format loosely follows
Conventional Commits; each entry: context, change (paths), evidence, impact, rollback.

## Unreleased

### feat: enable battery ADC sensing + activity LED for POCKET_MARAUDER
- Context: board has a LiPo path (TP4056/DW01A/FS8205A) with a voltage divider on IO34 (net `D34`) and a green GPIO LED on IO19 (net `LED`, active-low). Firmware had battery/LED disabled for this target; user wants a battery state indicator + percentage on the OLED and the LED driven by firmware.
- Change:
  - `firmware/esp32_marauder/configs.h`: POCKET_MARAUDER feature block (~148-152) — added `HAS_BATTERY`, `BATTERY_ADC_PIN 34`, `BATTERY_ADC_MULTIPLIER_X100 502`, `HAS_ACT_LED`, removed stale "no battery" comment; ACT LED STUFF block (~3318) — `ACT_LED_PIN 19` + `ACT_LED_ACTIVE_LOW` and `ACT_LED_ON`/`ACT_LED_OFF` polarity macros (stock boards keep active-high).
  - `firmware/esp32_marauder/BatteryInterface.cpp`: ADC path sets `i2c_supported = true` so `MenuFunctions::battery()` renders the percentage on mini/OLED screens (previously gated on i2c fuel gauge only); replaced hardcoded `* 2` divider scale with `BATTERY_ADC_MULTIPLIER_X100 / 100` (5.02x for R3=10k / R4=40.2k, matches this board; stock boards unaffected when they define their own pin).
  - `firmware/esp32_marauder/esp32_marauder.ino` + `WiFiScan.cpp`: the 4 `digitalWrite(ACT_LED_PIN, LOW/HIGH)` sites now use `ACT_LED_ON`/`ACT_LED_OFF` so the active-low LED lights while scanning (was inverted on this board).
- Evidence: `python -m platformio run -e pocket_marauder` -> `SUCCESS`, RAM 24.9% (81716/327680), Flash 81.4% (1599767/1966080). Divider verified from schematic `PCB/EAGLE/espmarauder.sch`: R3=10k to GND, R4=40.2k to switched cell rail (net labelled `+5V`, electrically the battery V+ after BATT_SW feeding boost IC2 VIN), tap to IO34.
- Impact: OLED status bar shows battery % (green >20%, red otherwise) polled every 3s; activity LED on IO19 lights during WiFi scan/attack, off otherwise.
- Rollback: remove the 5 lines from the POCKET_MARAUDER feature block, the `ACT_LED_PIN 19`/polarity block from ACT LED STUFF, revert the two `BatteryInterface.cpp` lines and the 4 `digitalWrite` sites back to `LOW`/`HIGH`.

### feat: add PlatformIO build env for POCKET_MARAUDER
- Context: board needs Arduino-ESP32 3.x / IDF 5.x (configs.h defines `HAS_IDF_3`, `HAS_NIMBLE_2`); stock PlatformIO `espressif32` is frozen on core 2.x. Provide a reproducible device build alongside the existing `[env:native]` Unity host tests.
- Change:
  - `firmware/platformio.ini`: NEW additive `[env:pocket_marauder]` on the pioarduino platform fork (`platform-espressif32` release `55.03.312-1`, Arduino 3.3.12 / IDF 5.5.5), `board = esp32dev`, `framework = arduino`, `board_build.partitions = min_spiffs.csv`, `lib_extra_dirs` for vendored libs, `build_src_filter` excluding `libraries/` from the src builder, `build_flags` `-Wno-error=return-type -fcommon -Wl,--allow-multiple-definition` (upstream relies on pre-GCC10 defaults + ODR clashes), `extra_scripts = pre:tools/pio_framework_libs.py`. `[env:native]` untouched.
  - NEW `firmware/tools/pio_framework_libs.py`: adds every bundled framework library `src` dir to the global include path (arduino-esp32 3.x libs cross-include with no `depends=`), and force-builds+links the `Network` library that LDF misses (WiFi reaches it only via a quoted include).
  - `firmware/esp32_marauder/configs.h`: added `SCREEN_BUFFER`/`MAX_SCREEN_BUFFER 6` and a `POCKET_MARAUDER` MENU DEFINITIONS block (`BANNER_TIME`, `COMMAND_PREFIX`, `KEY_*`, `BUTTON_PADDING`) the build required.
  - `firmware/esp32_marauder/OledAdapter.h`: `setTextWrap()`, `getTextBounds` `uint16_t w,h` signature fixes for the 3.x Adafruit_GFX.
- Evidence: `python -m platformio run -e pocket_marauder` -> `SUCCESS`, RAM 24.9% (81620/327680), Flash 80.9% (1590663/1966080), `firmware.bin` + combined `firmware.factory.bin` created.
- Impact: reproducible device firmware build; host-test env unaffected.
- Rollback: delete `[env:pocket_marauder]` and `firmware/tools/pio_framework_libs.py`; revert the configs.h/OledAdapter.h hunks.

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
