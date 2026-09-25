# PocketMarauder — Review & Plan

Updated: 2026-09-25
Status: PLAN — belum eksekusi, nunggu approval

## 1. Review Hardware (dari EAGLE sch + BOM + gerber)

Board: **espmarauder v2 / "PocketMarauder"** — SlimeVR by Positron Elektronik.
Sumber: `PCB/EAGLE/espmarauder.sch` (EAGLE 9.6.2), BOM `CAMOutputs/Assembly/espmarauder.txt`,
gerber `espmarauder_production_2026-03-05.zip`.

### Fakta terverifikasi
- **MCU**: ESP32-WROOM-32D (single unit; board produksi = 2 unit identik di-panel, konfirmasi user).
- **USB-UART**: FT231XS-R (auto-program via DTR/RTS Q1/Q2).
- **Power**: TP4056 charger + DW01A/FS8205A Li-ion protection + M3406-ADJ buck 3.3V + SI2301 P-MOSFET load switch. JST-PH2 battery + slide switch BATT_SW.
- **Display**: **OLED 128x64 I2C SSD1306** (`DISPLAY-OLED-128X64-I2C`, U$1).
  - SDA = **IO21**, SCL = **IO22**.
- **Buttons** (4x tactile 4x4, active-low, pull-up eksternal 10k):
  | Fungsi fisik | Net | GPIO | Pull-up |
  |---|---|---|---|
  | BTN A | D35 | **IO35** | R6 10k |
  | BTN B | D32 | **IO32** | R11 10k |
  | BTN C | D33 | **IO33** | R18 10k |
  | BTN D | D25 | **IO25** | R19 10k |
  - Catatan: net `BUTTON1` ada di **IO18** tapi tanpa komponen tombol → sisa template, abaikan.
  - IO35 = input-only (OK, pakai pull-up eksternal, tak bisa internal).
- **LED user**: LED = **IO19** (R9 330), LED2 = **IO26** (R20 330). Plus CHRG/STBY/RX/TX status LED (hardware, bukan GPIO).
- **microSD**: **TIDAK ADA** konektor di BOM. Net `SD_HOST_*` (IO2/4/12/13/14/15) = sisa template → matikan (konfirmasi user).
- **UART2** IO16/IO17 (RX_DWM/TX_DWM): bekas project lain → abaikan (konfirmasi user).
- **Board**: 2-layer, 80.0 x 48.5 mm, tebal 1.57mm. Gerber siap fab (EAGLE CAM, 13 file + drill).

## 2. Review Firmware (upstream justcallmekoko/ESP32Marauder v1.17.0)

### BLOCKER kritis — display incompatible
Upstream Marauder **hanya support TFT warna via TFT_eSPI** (ILI9341/ST7735/ST7789/CYD dst).
**Tidak ada driver OLED SSD1306 / monochrome sama sekali** di seluruh codebase
(cek: 0 match `SSD1306|SH1106|U8g2|Adafruit_GFX`).

`Display` class terikat erat ke API TFT_eSPI:
- `tft.drawString/setFreeFont/setTextColor`, `TFT_eSPI_Button`, warna 16-bit RGB565.
- Layout menu diasumsikan min ~135x240 px berwarna. OLED cuma 128x64 mono.

→ **Remap GPIO saja TIDAK cukup.** Board ini butuh backend display baru.

### Yang cocok tanpa modif besar
- WiFi/BT attack core (WiFiScan, EvilPortal, CommandLine) hardware-agnostik.
- `HAS_SCREEN` bisa dimatikan → firmware jalan **headless** (kontrol via serial CLI 115200).
- Build target `GENERIC_ESP32` sudah ada (screen off, button off) sebagai basis.

## 3. Arsitektur Display (KEPUTUSAN: Opsi B)
| Opsi | Effort | Hasil | Risiko |
|---|---|---|---|
| **A. Headless/CLI** | Kecil | Semua fitur attack via serial; OLED tak dipakai / status ringkas manual | Tak ada menu di device — kurang "produk" |
| **B. OLED backend baru** | Besar | Menu native di OLED 128x64 + 4 tombol | Nulis ulang layer Display (drawString, button, scroll) untuk SSD1306; port paling berat |
| **C. Fork komunitas OLED** | Sedang | Pakai fork Marauder yg sudah OLED (mis. varian 0.96") lalu remap | Perlu cari fork yg maintained & sinkron fitur; lisensi/quality bervariasi |

### KEPUTUSAN USER (2026-09-25)
- **Opsi B dipilih** — OLED backend baru (SSD1306 128x64 + 4 tombol nav).
- **Branding**: nama produk = **PocketMarauder**.
- **Boot logo**: foto French Bulldog (`local://image-c9ef49c3d688ab6a.png`, 433x577 PNG).
  → konversi ke bitmap 1-bit 128x64 (XBM), threshold/dither, di Fase 2.
- **Fitur ekstra**: user minta saran (lihat §3b).

## 3b. Saran Fitur (di luar Marauder standar)

Konteks board: OLED 128x64 mono, 4 tombol, battery Li-ion + TP4056, tanpa SD/GPS.
Semua yg butuh SD/GPS di upstream otomatis di-disable.

Prioritas layak (murah, cocok hardware ini):
1. **Battery indicator di status bar** — baca VBAT via ADC divider bila ada net battery-sense; kalau tak ada tap ADC, tampilkan status charging dari pin TP4056 CHRG/STBY (bila terhubung GPIO). Cek dulu di sch apakah ada.
2. **Nav 4-tombol yang enak** — mapping Up/Down/Select/Back (bukan 5-arah), karena cuma 4 tombol. Long-press = Back global.
3. **Boot logo + versi + nama** — splash PocketMarauder 2 detik.
4. **Sleep/backlight-off timeout** — OLED aging + hemat baterai; wake on button.

Opsional (nice-to-have):
5. Menu "Favorites" — shortcut ke 2-3 scan yg paling sering (deauth detector, beacon spam, packet monitor) biar cepat di layar kecil.
6. Indikator RAM/heap ringkas di status bar (Marauder rentan OOM).

Dihindari (tak cocok / tak ada hardware):
- Wardriving/GPS log (tak ada GPS + tak ada SD buat simpan).
- PCAP ke SD (tak ada SD) — hanya live serial dump.
### Fase 1 — Board config
- [ ] Tambah target `POCKET_MARAUDER` di `firmware/esp32_marauder/configs.h`:
  - Buttons: L/C/U/D/R → IO35/IO32/IO33/IO25 (+ mapping ke navigasi menu).
  - `HAS_BUTTONS`, `HAS_BT`, matikan `HAS_SD`/`HAS_GPS`.
  - I2C: SDA=21, SCL=22.
- [ ] Boot splash + `HARDWARE_NAME "PocketMarauder"` (branding).
- Bukti: compile config-only, cek makro resolve.

### Fase 2 — Display (sesuai opsi terpilih A/B/C)
- A: `HAS_SCREEN` off, verifikasi CLI. Bukti: serial log scan WiFi.
- B: implement `DisplayOLED` (SSD1306 via Adafruit_SSD1306/U8g2), adaptor menu. Bukti: foto OLED render menu.

### Fase 3 — Build & flash
- [ ] Build via arduino-cli (toolchain terdeteksi: `arduino-cli` ada; PlatformIO belum).
- [ ] Flash 1 unit, smoke test: boot → menu/CLI → 1 scan WiFi sukses.
- [ ] Dokumentasi prosedur flash produksi (2 unit per panel).

### Fase 4 — Cleanup
- [ ] Hapus `_tmp_marauder`, file bekas, update CHANGELOG.

## 5. Toolchain terdeteksi
- `arduino-cli` ✓ (`C:\Program Files\Arduino CLI`)
- `git`, `python3` ✓
- PlatformIO ✗ (upstream punya `platformio.ini` tapi cuma buat env `native` test; build device pakai Arduino IDE/CLI).

## 6. Keputusan yang ditunggu dari user
1. **Opsi display A / B / C?** (paling menentukan scope).
2. Branding: nama produk final, boot logo custom? (punya aset sendiri?).
3. Fitur firmware di luar Marauder standar yang mau ditambah? (blm ada, konfirmasi).

## Don't repeat
- Upstream TIDAK punya OLED driver — jangan asumsikan remap GPIO cukup untuk display.
- Board TANPA microSD & GPS meski net template ada.
