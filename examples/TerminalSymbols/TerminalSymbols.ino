/*
 * TerminalSymbols - every LVGL symbol the terminal's built-in font carries,
 * with its name, and a selection of the Unicode symbols, on the Waveshare
 * ESP32-P4-86-Panel.  LVGL's symbols print under LVGL's own names
 * (LV_SYMBOL_WIFI ...), so a line moves between the terminal and an LVGL
 * label unchanged.
 *
 * Try the other ways to run: set TERM_ROTATION (0, 90, 180, 270 -- the
 * picture turned clockwise; 270 puts the USB ports at the bottom) and
 * TERM_RESOLUTION (720, or 480 for larger text) below.  720 at 0 or 180 is
 * scrolled by the display DMA alone; the rest use the PPA.
 *
 * Arduino IDE Board Settings:
 *   - Board: ESP32P4 Dev Module
 *   - PSRAM: Enabled
 *   - Flash Size: 32MB (256Mb)
 *   - Partition Scheme: Custom (uses the partitions.csv in this sketch folder)
 *   - USB CDC On Boot: Disabled  <- bottom "USB TO UART" port (CH343P into UART0)
 *
 * Copyright (c) 2026 chipguyhere
 * MIT License
 */

#include <chipguy_P486Panel_terminal.h>

chipguy_P486Panel_display  display;
chipguy_P486Panel_terminal term;

#define TERM_ROTATION   0      // 0, 90, 180 or 270
#define TERM_RESOLUTION 720    // 720, or 480 for larger text

struct Sym { const char *glyph, *name; };
#define SYM(n) {LV_SYMBOL_##n, #n}
static const Sym kSymbols[] = {
    SYM(BULLET), SYM(AUDIO), SYM(VIDEO), SYM(LIST), SYM(OK), SYM(CLOSE),
    SYM(POWER), SYM(SETTINGS), SYM(HOME), SYM(DOWNLOAD), SYM(DRIVE),
    SYM(REFRESH), SYM(MUTE), SYM(VOLUME_MID), SYM(VOLUME_MAX), SYM(IMAGE),
    SYM(TINT), SYM(PREV), SYM(PLAY), SYM(PAUSE), SYM(STOP), SYM(NEXT),
    SYM(EJECT), SYM(LEFT), SYM(RIGHT), SYM(PLUS), SYM(MINUS), SYM(EYE_OPEN),
    SYM(EYE_CLOSE), SYM(WARNING), SYM(SHUFFLE), SYM(UP), SYM(DOWN), SYM(LOOP),
    SYM(DIRECTORY), SYM(UPLOAD), SYM(CALL), SYM(CUT), SYM(COPY), SYM(SAVE),
    SYM(BARS), SYM(ENVELOPE), SYM(CHARGE), SYM(PASTE), SYM(BELL),
    SYM(KEYBOARD), SYM(GPS), SYM(FILE), SYM(WIFI), SYM(BATTERY_FULL),
    SYM(BATTERY_3), SYM(BATTERY_2), SYM(BATTERY_1), SYM(BATTERY_EMPTY),
    SYM(USB), SYM(BLUETOOTH), SYM(TRASH), SYM(EDIT), SYM(BACKSPACE),
    SYM(SD_CARD), SYM(NEW_LINE),
};

// Unicode symbols handy in embedded projects, also in the built-in font.
static const Sym kUnicode[] = {
    {"°", "degree"}, {"℃", "celsius"}, {"µ", "micro"},
    {"Ω", "ohm"}, {"±", "plus-minus"}, {"≈", "approx"},
    {"≤", "less-equal"}, {"≥", "greater-eq"}, {"✓", "check"},
    {"✗", "cross"}, {"⚠", "warning"}, {"⚡", "voltage"},
    {"←", "arrow left"}, {"→", "arrow right"}, {"▲", "triangle up"},
    {"●", "circle"}, {"█", "full block"}, {"▒", "shade"},
};

static void listOut(const Sym *list, size_t n, uint16_t nameColor) {
    for (size_t i = 0; i < n; i++) {
        term.setTextColor(P486T_WHITE);
        term.print(list[i].glyph);
        term.setTextColor(nameColor);
        term.printf(" %s\n", list[i].name);
        delay(120);
    }
}

void setup() {
    Serial0.begin(115200);
    if (!term.begin(display, TERM_ROTATION, TERM_RESOLUTION)) {
        Serial0.println("Display init failed!");
        while (1) delay(100);
    }
    term.setCursorStyle(P486T_CURSOR_OFF);
}

void loop() {
    term.setTextColor(P486T_YELLOW);
    term.println("LV_SYMBOL_...");
    listOut(kSymbols, sizeof kSymbols / sizeof kSymbols[0], P486T_CYAN);
    term.setTextColor(P486T_YELLOW);
    term.println("Unicode");
    listOut(kUnicode, sizeof kUnicode / sizeof kUnicode[0], P486T_GREEN);
    delay(3000);
}
