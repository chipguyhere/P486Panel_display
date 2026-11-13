/*
 * TerminalTouchButtons - touch buttons made of colored text, on the text
 * terminal of the Waveshare ESP32-P4-86-Panel.
 *
 * Each button is a run of character cells: a half block at each end and a
 * label in between.  pollTouch() reports a touch in the same column / row
 * terms as setCursor(), so "was it touched?" is a simple comparison.  Two
 * buttons switch the board's relays (through the board library), the third
 * dims the backlight; a pressed button shows lit while your finger is on it.
 *
 * The relays click (their red LEDs are inside the enclosure).  Disconnect anything wired to
 * the relay terminals before running this.
 *
 * The buttons are ordinary text, so printing enough to scroll would scroll
 * the buttons away with everything else -- for buttons that stay put while
 * text scrolls, put them in a frozen area (see TerminalHeaderFooter) and test
 * touches with P486T_TOUCH_PIXELS.
 *
 * Try the other ways to run: set TERM_ROTATION (0, 90, 180, 270 -- the
 * picture turned clockwise; 270 puts the USB ports at the bottom) and
 * TERM_RESOLUTION (720, or 480 for larger text) below.  Touch follows.
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

#include <P486Panel_Board.h>
#include <chipguy_P486Panel_terminal.h>

P486Panel_Board            board;                 // shared I2C bus, relays
chipguy_P486Panel_display  display;
chipguy_P486Panel_touch    touch(board.Wire);     // the touch chip is on the board's bus
chipguy_P486Panel_terminal term;

#define TERM_ROTATION   0      // 0, 90, 180 or 270
#define TERM_RESOLUTION 720    // 720, or 480 for larger text

enum Action : uint8_t { RELAY1, RELAY2, DIM };

struct Button {
    int16_t col, row, width;
    const char *label;
    uint16_t color;
    Action action;
    bool on;
};

static Button buttons[3];

static void drawButton(const Button &b, bool pressed) {
    // Off: the color's label on grey.  On: black on the color.  Pressed:
    // white on the color, so a press shows before you let go.
    uint16_t fill = (pressed || b.on) ? b.color : P486T_GREY;
    uint16_t text = pressed ? P486T_WHITE : b.on ? P486T_BLACK : b.color;
    term.setCursor(b.col, b.row);
    term.setTextColor(fill, P486T_BLACK);
    term.print("\u2590");                    // ▐ right half block: the left end, half a cell in
    term.setTextColor(text, fill);
    term.print(b.label);
    term.setTextColor(fill, P486T_BLACK);
    term.print("\u258C");                    // ▌ left half block: the right end
    term.setTextColor(P486T_WHITE, P486T_BLACK);
}

static void status(int16_t row, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    term.setCursor(0, row);
    term.vprintf(fmt, ap);
    term.print(P486T_CLEAR_EOL_STR);
    va_end(ap);
}

static int hit(int16_t col, int16_t row) {
    for (int i = 0; i < 3; i++) {
        const Button &b = buttons[i];
        if (row == b.row && col >= b.col && col < b.col + b.width) return i;
    }
    return -1;
}

void setup() {
    Serial0.begin(115200);
    if (!term.begin(display, TERM_ROTATION, TERM_RESOLUTION)) {
        Serial0.println("Display init failed!");
        while (1) delay(100);
    }
    board.begin();
    if (!touch.begin()) Serial0.println("Touch controller did not answer");
    term.attachTouch(touch);
    term.setCursorStyle(P486T_CURSOR_OFF);

    // Three buttons of six cells (half block, four letters, half block)
    // spread across the screen.
    int16_t gap = (term.columns() - 6) / 2;
    buttons[0] = {0,                  2, 6, "RLY1", P486T_RED,    RELAY1, false};
    buttons[1] = {gap,                2, 6, "RLY2", P486T_ORANGE, RELAY2, false};
    buttons[2] = {(int16_t)(2 * gap), 2, 6, "DIM ", P486T_CYAN,   DIM,    false};
    for (Button &b : buttons) drawButton(b, false);
    term.setTextColor(P486T_YELLOW);
    term.setCursor(0, 0);
    term.print("Relays and backlight");
    term.setTextColor(P486T_WHITE);
    status(4, "Tap a button");
}

void loop() {
    static int pressed = -1;
    int16_t col, row, px, py;
    bool down = term.pollTouch(col, row);
    if (down) {
        term.pollTouch(px, py, P486T_TOUCH_PIXELS);
        status(6, "cell %d,%d", col, row);
        status(7, "px %d,%d", px, py);
    }
    int now = down ? hit(col, row) : -1;
    if (now != pressed) {
        if (pressed >= 0 && !down) {
            // Lifted off the button (not slid off it): toggle it.
            Button &b = buttons[pressed];
            b.on = !b.on;
            switch (b.action) {
                case RELAY1: board.setRelay(0, b.on); break;
                case RELAY2: board.setRelay(1, b.on); break;
                case DIM:    display.setBacklight(b.on ? 20 : 100); break;
            }
            status(4, "%s %s", b.label, b.on ? "on" : "off");
        }
        if (pressed >= 0) drawButton(buttons[pressed], false);
        if (now >= 0) drawButton(buttons[now], true);
        pressed = now;
    }
    delay(20);
}
