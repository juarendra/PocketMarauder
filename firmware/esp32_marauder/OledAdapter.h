// OledAdapter.h
//
// SSD1306 128x64 monochrome OLED backend for the POCKET_MARAUDER target.
//
// The rest of the firmware is written against the TFT_eSPI color-display
// surface: `display_obj.tft.<method>(...)` with RGB565 colors, the TFT_*
// color/datum macros, and `display_obj.key[b]` (TFT_eSPI_Button). This file
// maps that exact API onto the vendored Adafruit_SSD1306 (1-bit framebuffer):
//
//   - RGB565 0x0000 (TFT_BLACK) -> pixel off, any other color -> pixel on
//   - <TFT_eSPI.h> is not included for the OLED target, so the TFT_* macros
//     it normally provides are defined here and TFT_eSPI_Button is replaced
//     by OledButton
//   - every draw method flushes the framebuffer to the panel immediately
//     (the TFT path updates while drawing)
//
// ponytail: flush after every draw/print call. A full 128x64 frame is ~1 KB
// over 400 kHz I2C (~2-3 ms), so a menu redraw costs tens of ms. If boot/menu
// feels sluggish, upgrade to a dirty flag on OledDisplay + one display() per
// frame in the Display tick.

#pragma once

#ifdef POCKET_MARAUDER

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// TFT_eSPI-compatible macros.
// Values match TFT_eSPI.h exactly; only zero-vs-nonzero affects actual
// drawing (the mono mapping), but keeping the real values means equality
// comparisons (node.color == TFT_GREEN, ...) behave identically.
// ---------------------------------------------------------------------------

// The PocketMarauder has no TFT hardware, but the firmware references these
// pins under #ifdef HAS_SCREEN. -1 keeps pinMode()/digitalWrite() as no-ops.
#define TFT_BL -1
#define TFT_CS -1

#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_GREENYELLOW 0xB7E0
#define TFT_LIGHTGREY   0xD69A
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFDA0
#define TFT_BROWN       0x9A60
#define TFT_SILVER      0xC618
#define TFT_SKYBLUE     0x867D
#define TFT_VIOLET      0x915C

// Text datums (TFT_eSPI encoding), used by OledButton and setLabelDatum().
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define CL_DATUM 3 // same as ML_DATUM in TFT_eSPI
#define MC_DATUM 4
#define MR_DATUM 5
#define CR_DATUM 5 // same as MR_DATUM in TFT_eSPI
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

// ---------------------------------------------------------------------------
// OledDisplay - Adafruit_SSD1306 plus the TFT_eSPI methods the firmware
// calls. Inheriting (rather than wrapping) picks up the full Print surface,
// setCursor, getCursorY, width/height, setRotation and display() for free.
// ---------------------------------------------------------------------------
class OledDisplay : public Adafruit_SSD1306 {
 public:
  OledDisplay()
      : Adafruit_SSD1306(TFT_WIDTH, TFT_HEIGHT, &Wire, OLED_RST) {}

  // TFT_eSPI::init() equivalent. Display::init()/RunSetup() and
  // MenuFunctions::changeMenu() call it on every menu switch, so keep it
  // idempotent (re-init would blank the screen and re-run the controller
  // sequence).
  void init() {
    if (oled_inited) {
      return;
    }
    Wire.begin(OLED_SDA, OLED_SCL);
    oled_inited = begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  }

  // Map an RGB565 color to the panel's 1-bit value.
  static inline uint16_t mono(uint16_t c) {
    return c ? SSD1306_WHITE : SSD1306_BLACK;
  }

  // --- TFT-style text configuration ----------------------------------------
  // The base setTextColor() would take raw RGB565; these shadows map through
  // mono() first. The background and inverse arguments are accepted for API
  // compatibility but ignored (a 1-bit panel has no per-pixel background).
  void setTextColor(uint16_t c) { Adafruit_GFX::setTextColor(mono(c)); }
  void setTextColor(uint16_t fg, uint16_t bg) {
    (void)bg;
    Adafruit_GFX::setTextColor(mono(fg));
  }
  void setTextColor(uint16_t fg, uint16_t bg, bool inverse) {
    (void)bg;
    (void)inverse;
    Adafruit_GFX::setTextColor(mono(fg));
  }
  // TFT_eSPI allows a second "word wrap" flag; the OLED wrap handling only
  // needs the first one.
  void setTextWrap(bool wrap, bool wordwrap) {
    (void)wordwrap;
    Adafruit_GFX::setTextWrap(wrap);
  }
  // MENU_FONT is NULL on this target so free fonts are never requested;
  // forward to the base (NULL = default 5x7 GLCD font).
  void setFreeFont(const GFXfont *f) { Adafruit_GFX::setFont(f); }

  // Adafruit_GFX has no textWidth(); measure it with getTextBounds().
  int16_t textWidth(const char *s) {
    int16_t x1, y1, w, h;
    Adafruit_GFX::getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return w;
  }
  int16_t textWidth(const __FlashStringHelper *s) {
    int16_t x1, y1, w, h;
    Adafruit_GFX::getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return w;
  }
  int16_t textWidth(const String &s) {
    int16_t x1, y1, w, h;
    Adafruit_GFX::getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return w;
  }

  // --- TFT_eSPI-compatible string drawing ------------------------------------
  // The trailing font argument is accepted for API compatibility; the OLED
  // always renders in the current (default 5x7) font. y is the text top, as
  // with TFT_eSPI's default TL datum. x semantics: left edge / centre / right
  // edge respectively.
  void drawString(const char *s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s, x, y, 0);
  }
  void drawString(const String &s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s.c_str(), x, y, 0);
  }
  void drawCentreString(const char *s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s, x, y, -textWidth(s) / 2);
  }
  void drawCentreString(const String &s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s.c_str(), x, y, -textWidth(s) / 2);
  }
  void drawRightString(const char *s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s, x, y, -textWidth(s));
  }
  void drawRightString(const String &s, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    drawText(s.c_str(), x, y, -textWidth(s));
  }

  // --- Shape primitives ------------------------------------------------------
  // Same signatures as Adafruit_GFX; each maps the color to mono and flushes
  // the panel after drawing (see the ponytail note at the top of the file).
  void fillScreen(uint16_t color) override {
    Adafruit_GFX::fillScreen(mono(color));
    display();
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    Adafruit_GFX::fillRect(x, y, w, h, mono(color));
    display();
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h,
                     uint16_t color) override {
    Adafruit_GFX::drawFastVLine(x, y, h, mono(color));
    display();
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w,
                     uint16_t color) override {
    Adafruit_GFX::drawFastHLine(x, y, w, mono(color));
    display();
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                uint16_t color) override {
    Adafruit_GFX::drawLine(x0, y0, x1, y1, mono(color));
    display();
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    Adafruit_GFX::drawRect(x, y, w, h, mono(color));
    display();
  }
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color) {
    Adafruit_GFX::drawRoundRect(x, y, w, h, r, mono(color));
    display();
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color) {
    Adafruit_GFX::fillRoundRect(x, y, w, h, r, mono(color));
    display();
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    Adafruit_GFX::fillCircle(x, y, r, mono(color));
    display();
  }
  void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w,
                   int16_t h, uint16_t color) {
    Adafruit_GFX::drawXBitmap(x, y, bitmap, w, h, mono(color));
    display();
  }

  // --- Print shadows ----------------------------------------------------------
  // Print::print()/println() only stage text into the GFX framebuffer, so
  // flush after each call to keep the screen in sync. Only the overloads the
  // firmware actually uses are shadowed; any other Print overload falls
  // through to the base and will flush on the next draw/print.
  size_t print(const String &s) {
    size_t n = Print::print(s);
    display();
    return n;
  }
  size_t print(const char *s) {
    size_t n = Print::print(s);
    display();
    return n;
  }
  size_t print(const __FlashStringHelper *s) {
    size_t n = Print::print(s);
    display();
    return n;
  }
  size_t print(char c) {
    size_t n = Print::print(c);
    display();
    return n;
  }
  size_t print(int n, uint8_t base = DEC) {
    size_t r = Print::print(n, base);
    display();
    return r;
  }
  size_t print(unsigned int n, uint8_t base = DEC) {
    size_t r = Print::print(n, base);
    display();
    return r;
  }
  size_t print(long n, uint8_t base = DEC) {
    size_t r = Print::print(n, base);
    display();
    return r;
  }
  size_t print(unsigned long n, uint8_t base = DEC) {
    size_t r = Print::print(n, base);
    display();
    return r;
  }
  size_t print(double n, uint8_t digits = 2) {
    size_t r = Print::print(n, digits);
    display();
    return r;
  }
  size_t print(float n, uint8_t digits = 2) {
    size_t r = Print::print(n, digits);
    display();
    return r;
  }
  size_t println(void) {
    size_t n = Print::println();
    display();
    return n;
  }
  size_t println(const String &s) {
    size_t n = Print::println(s);
    display();
    return n;
  }
  size_t println(const char *s) {
    size_t n = Print::println(s);
    display();
    return n;
  }
  size_t println(const __FlashStringHelper *s) {
    size_t n = Print::println(s);
    display();
    return n;
  }
  size_t println(char c) {
    size_t n = Print::println(c);
    display();
    return n;
  }
  size_t println(int n, uint8_t base = DEC) {
    size_t r = Print::println(n, base);
    display();
    return r;
  }
  size_t println(unsigned int n, uint8_t base = DEC) {
    size_t r = Print::println(n, base);
    display();
    return r;
  }
  size_t println(long n, uint8_t base = DEC) {
    size_t r = Print::println(n, base);
    display();
    return r;
  }
  size_t println(unsigned long n, uint8_t base = DEC) {
    size_t r = Print::println(n, base);
    display();
    return r;
  }
  size_t println(double n, uint8_t digits = 2) {
    size_t r = Print::println(n, digits);
    display();
    return r;
  }
  size_t println(float n, uint8_t digits = 2) {
    size_t r = Print::println(n, digits);
    display();
    return r;
  }

 private:
  bool oled_inited = false;

  // Draw `s` at (x + shift, y) with the current color/size, then flush.
  void drawText(const char *s, int32_t x, int32_t y, int16_t shift) {
    Adafruit_GFX::setCursor((int16_t)(x + shift), (int16_t)y);
    Print::print(s);
    display();
  }
};

// ---------------------------------------------------------------------------
// OledButton - drop-in replacement for TFT_eSPI_Button (same public API,
// 1-bit rendering).
// ---------------------------------------------------------------------------
class OledButton {
 public:
  // "Classic" initButton(): x/y is the button centre, w/h the size.
  void initButton(OledDisplay *gfx, int16_t x, int16_t y, uint16_t w,
                  uint16_t h, uint16_t outline, uint16_t fill,
                  uint16_t textcolor, char *label, uint8_t textsize) {
    _gfx = gfx;
    _x1 = x - (w / 2);
    _y1 = y - (h / 2);
    _w = w;
    _h = h;
    _outlinecolor = outline;
    _fillcolor = fill;
    _textcolor = textcolor;
    _textsize = textsize;
    if (label) {
      strncpy(_label, label, 9);
    } else {
      _label[0] = '\0';
    }
    _label[9] = '\0';
  }

  // Adjust text datum and x/y deltas (relative to the button centre).
  void setLabelDatum(int16_t x_delta, int16_t y_delta,
                     uint8_t datum = MC_DATUM) {
    _xd = x_delta;
    _yd = y_delta;
    _textdatum = datum;
  }

  void drawButton(bool inverted = false, String long_name = "") {
    uint16_t fill, outline, text;
    if (!inverted) {
      fill = _fillcolor;
      outline = _outlinecolor;
      text = _textcolor;
    } else {
      fill = _textcolor;
      outline = _outlinecolor;
      text = _fillcolor;
    }

    int16_t r = min(_w, _h) / 4;  // corner radius
    _gfx->fillRoundRect(_x1, _y1, _w, _h, r, fill);
    _gfx->drawRoundRect(_x1, _y1, _w, _h, r, outline);

    const char *label = (long_name == "") ? _label : long_name.c_str();
    if (label[0] == '\0') {
      return;
    }

    // Label position mirrors TFT_eSPI_Button::drawButton: datum point at
    // (centre + offset); MC centres the text there, ML left-aligns it.
    int16_t cx = _x1 + (_w / 2) + _xd;
    int16_t text_w = _gfx->textWidth(label);
    int16_t tx = (_textdatum == MC_DATUM) ? cx - text_w / 2 : cx;
    int16_t ty = _y1 + (_h / 2) - 4 * _textsize + _yd;

    _gfx->setTextSize(_textsize);
    _gfx->setTextColor(text);
    _gfx->drawString(label, tx, ty, 1);
  }

  bool contains(int16_t x, int16_t y) {
    return (x >= _x1) && (x < (_x1 + _w)) && (y >= _y1) && (y < (_y1 + _h));
  }

  void press(bool p) {
    laststate = currstate;
    currstate = p;
  }

  bool isPressed() { return currstate; }
  bool justPressed() { return (currstate && !laststate); }
  bool justReleased() { return (!currstate && laststate); }

 private:
  OledDisplay *_gfx = nullptr;
  int16_t _x1 = 0, _y1 = 0;
  int16_t _xd = 0, _yd = 0;
  uint16_t _w = 0, _h = 0;
  uint8_t _textsize = 1;
  uint8_t _textdatum = MC_DATUM;
  uint16_t _outlinecolor = 0, _fillcolor = 0, _textcolor = 0;
  char _label[10];

  bool currstate = false;
  bool laststate = false;
};

#endif  // POCKET_MARAUDER
