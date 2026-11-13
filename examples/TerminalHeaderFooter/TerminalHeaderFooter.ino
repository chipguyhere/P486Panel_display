/*
 * TerminalHeaderFooter - a fixed header and footer with text scrolling
 * between them, on the text terminal of the Waveshare ESP32-P4-86-Panel.
 *
 * The terminal can leave strips at the top and bottom of the screen out of
 * the scroll ("frozen").  This sketch freezes 60 pixels at the top and 60 at
 * the bottom, draws a pre-rendered header and footer image into them with
 * drawBitmap(), and prints a log in between.  While the text scrolls, it
 * keeps updating an "activity" bar inside the footer -- drawing into a frozen
 * area never disturbs the scrolling text.  At 720 x 720 and rotation 0 or
 * 180 the display DMA itself reads the screen in pieces -- header, the
 * scrolling window (two pieces where it wraps), footer -- so nothing is
 * copied to scroll.
 *
 * The images are RGB565 arrays in bitmaps.h, 720 pixels wide, made by
 * extras/bitmaps/make_header_footer.py.  Make your own the same way: any
 * w x h array of uint16_t RGB565 pixels, row by row, will do.  At resolution
 * 480 they are drawn centered and lose 120 pixels at each side.
 *
 * Header and footer are each two text lines tall (60 pixels), so the 600
 * pixels between them hold exactly 20 lines -- 12 at resolution 480.  Freeze
 * a height that isn't a multiple of 30 and the top line can end up partly
 * cut off; evictPartialLine() scrolls just enough to push it off, and this
 * sketch shows the call.
 *
 * Try the other ways to run: set TERM_ROTATION (0, 90, 180, 270 -- the
 * picture turned clockwise; 270 puts the USB ports at the bottom) and
 * TERM_RESOLUTION (720, or 480 for larger text) below.
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
#include "bitmaps.h"

chipguy_P486Panel_display  display;
chipguy_P486Panel_terminal term;

#define TERM_ROTATION   0      // 0, 90, 180 or 270
#define TERM_RESOLUTION 720    // 720, or 480 for larger text

// Fill part of the footer's activity bar: `fill` of BAR_W pixels lit.
static void drawBar(int fill, uint16_t color) {
    uint16_t row[BAR_W];
    for (int x = 0; x < BAR_W; x++) row[x] = x < fill ? color : 0x2104;
    int16_t x0 = (term.width() - FOOTER_W) / 2 + BAR_X;
    int16_t y0 = term.height() - FOOTER_H + BAR_Y;
    for (int y = 0; y < BAR_H; y++) term.drawBitmap(x0, y0 + y, BAR_W, 1, row);
}

void setup() {
    Serial0.begin(115200);
    if (!term.begin(display, TERM_ROTATION, TERM_RESOLUTION)) {
        Serial0.println("Display init failed!");
        while (1) delay(100);
    }
    term.setCursorStyle(P486T_CURSOR_UNDERLINE);
    term.setScrollMode(P486T_SCROLL_INSTANT);   // full speed throughout

    // Freeze the strips, then draw into them.  Centered: at resolution 480
    // the 720-wide images lose 120 pixels at each side.
    term.freeze(HEADER_H, FOOTER_H);
    int16_t x = (term.width() - HEADER_W) / 2;
    term.drawBitmap(x, 0, HEADER_W, HEADER_H, kHeader);
    term.drawBitmap(x, term.height() - FOOTER_H, FOOTER_W, FOOTER_H, kFooter);
    drawBar(0, P486T_GREEN);

    term.printf("%d x %d cells\nbetween the\nheader and footer\n", term.columns(), term.rows());
}

void loop() {
    static uint32_t n = 0;

    // A long burst of log lines as fast as the terminal takes them, with the
    // footer bar tracking progress.
    const int kLines = 400;
    term.flush();
    uint32_t t0 = millis(), px0 = term.pixelsScrolled();
    for (int i = 0; i < kLines; i++) {
        term.setTextColor(i % 7 ? P486T_WHITE : P486T_YELLOW);
        term.printf("line %lu\n", (unsigned long)n++);
        if (i % 10 == 9) drawBar((i + 1) * BAR_W / kLines, P486T_GREEN);
    }
    term.flush();
    float lps = (term.pixelsScrolled() - px0) * 1000.0f / (millis() - t0) / term.cellHeight();

    // Scrolling speed.
    term.setTextColor(P486T_GREEN);
    term.printf("%.0f lines%sper second\n", lps, term.columns() >= 20 ? " " : "\n");
    delay(2000);

    // The top line is now probably cut in half by the header.
    term.setTextColor(P486T_CYAN);
    term.print("top line partial?");
    delay(1500);
    term.evictPartialLine();          // scroll on until it's gone
    term.print("\revicted" P486T_CLEAR_EOL_STR);
    delay(1500);
    term.evictPartialLine(true);      // or back: blank space comes in at the top
    term.print("\rscrolled back" P486T_CLEAR_EOL_STR);
    delay(1500);
    term.setTextColor(P486T_WHITE);
    term.println();
    drawBar(0, P486T_GREEN);
}
