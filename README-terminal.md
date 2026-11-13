# The text terminal

This is one of the two ways to drive the screen: a **text terminal**, like a computer's
console. You `print()` and `printf()`, and the text appears on a grid of character
cells and scrolls up the screen as it fills. The other way is [LVGL](README.md#the-examples),
which draws buttons, sliders and screens. Pick one per sketch: the terminal owns the
screen while it runs.

The terminal needs no other library and scrolls in hardware: the screen moves without
the text being drawn again. It's made for a **stream** of text — a log, a status feed,
a console. For readings that update in place, a dashboard in LVGL is usually the better
fit.

---

## Getting started

```cpp
#include <chipguy_P486Panel_terminal.h>

chipguy_P486Panel_display  display;
chipguy_P486Panel_terminal term;

void setup() {
    term.begin(display);              // starts the screen; it clears to black
    term.println("Hello, P486 Panel!");
    term.printf("%d x %d cells\n", term.columns(), term.rows());
}

void loop() {
}
```

`term.begin()` starts the display itself, so don't call `lv_setup.begin()` or
`display.begin()` as well. The screen is **45 characters wide and exactly 24 lines
tall**. Each cell is 16 × 30 pixels.

## Turning it, and making the text bigger

`begin()` takes two more settings:

```cpp
term.begin(display, 270);         // turned a quarter-turn: USB ports at the bottom
term.begin(display, 0, 480);      // larger text: 30 x 16 cells
term.begin(display, 270, 480);    // both
```

- **Rotation** is 0, 90, 180 or 270, the picture turned clockwise — the same as
  `display.setRotation()` for LVGL. 270 suits a panel mounted with its USB ports
  pointing down.
- **Resolution** is 720 (the screen's own) or 480. At 480 the terminal works on a
  480 × 480 screen and the board's graphics hardware stretches it to fill the panel,
  so every character is 1.5 times bigger — easier to read across a room.

Every combination works. They differ in what does the scrolling:

| Rotation, resolution | How it scrolls | Lines per second (measured) |
|---|---|---|
| 0 or 180, 720 | the display hardware reads the screen from a new place — nothing is copied | about 100 smooth, 270 instant |
| any, 480 | the graphics hardware (PPA) copies and stretches the screen each frame | about 100 |
| 90 or 270, 720 | the PPA copies and turns the screen each frame | about 50 |

The numbers are for text pouring in as fast as possible. Smooth scrolling is held to
about 100 lines a second on purpose, so you can still see the text move. A line printed
now and then scrolls in about a tenth of a second (a fifth in the turned 720 setting). The turned 720 setting is the
slowest because turning a whole 720 × 720 screen takes the PPA about 21 ms, longer
than one frame — [the engineering notes](README-engineering.md#the-text-terminal) have
the measurements.

## The examples

Open them from **File ▸ Examples ▸ chipguy_P486Panel_display**. Each has
`TERM_ROTATION` and `TERM_RESOLUTION` near the top to try the other settings.

| Example | What it shows |
|---|---|
| **TerminalHello** | The basics: colors, `printf`, tabs, symbols, box drawing, the cursor styles. Then it counts upward, faster and faster. **Start your project from this one.** |
| **TerminalScrollSpeed** | How fast it goes: bursts of a pretend log, too fast to read, with the speed shown after each. |
| **TerminalTouchButtons** | Buttons made of colored text that you tap; they switch the relays and dim the screen. |
| **TerminalHeaderFooter** | A picture at the top and bottom of the screen that stays put while text scrolls between them. |
| **TerminalSymbols** | Every LVGL symbol the terminal can print, with its name. |
| **TerminalMatrixRain** | The green "digital rain" from *The Matrix*, in half-width katakana. |

---

## Printing

`print()`, `println()`, `printf()` and `putc()` all work as they do with `Serial`.
Text is **UTF-8**, so you can write `"21.5°C"` or `"±5%"` straight into your sketch.

`printf()` never uses the heap. Width and padding work as usual —
`printf("%5d", n)`, `printf("%-8s", name)`, `printf("%06.2f", v)` — and for strings the
width counts screen cells, so columns line up even with characters that take two cells.
One limit: a single number longer than 47 characters (say `"%.60f"`) is cut short.

### Special characters

| Character | In a string | What it does |
|---|---|---|
| New line | `\n` | Go to the start of the next line, scrolling if needed. |
| Return | `\r` | Go back to the start of this line (handy for a counter that rewrites itself). |
| Tab | `\t` | Print spaces up to the next column that's a multiple of 4. |
| Backspace | `\b` | Move back one cell (doesn't erase). |
| Clear to end of line | `\v`, or `P486T_CLEAR_EOL_STR` | Erase from the cursor to the end of the line, in the current background color. |
| Clear screen | `\f` | Same as `clear()`. |

### Lines that fill the screen exactly

When text fills the last cell of a line, the cursor waits just past the right edge
instead of jumping to the next line. It only moves on when you print something more.
So a line exactly as wide as the screen, followed by `\n`, takes one line, not two.
`setImmediateWrap(true)` switches back to jumping at once.

## Colors

```cpp
term.setTextColor(P486T_YELLOW);                  // text color
term.setTextColor(P486T_BLACK, P486T_CYAN);       // text and background
term.setTextColor(p486t_rgb565(255, 128, 0));     // any color
```

Ready-made colors: `P486T_BLACK`, `WHITE`, `GREY`, `RED`, `GREEN`, `BLUE`, `YELLOW`,
`CYAN`, `MAGENTA`, `ORANGE`. Colors are standard RGB565 numbers, the same as in LVGL.

`clear()` fills the screen with the current **background** color and puts the cursor at
the top left.

## Where the text goes

Columns and lines are counted from 0 at the top left. `setCursor(col, row)` moves
there; `cursorCol()` and `cursorRow()` say where the cursor is; `columns()` and
`rows()` say how big the grid is.

**Row 0 is always the top line you can see** — even when only part of it is showing,
which happens when the screen isn't a whole number of lines tall. `evictPartialLine()`
scrolls on just far enough to push a cut-off top line out of sight.
`evictPartialLine(true)` scrolls back instead, pushing a cut-off bottom line out and
bringing blank space in at the top.

### The cursor

`setCursorStyle(P486T_CURSOR_BLOCK)` (the default), `P486T_CURSOR_UNDERLINE`,
`P486T_CURSOR_BAR` (a thin line at the left of the cell) or `P486T_CURSOR_OFF`. It
blinks by itself, in the current text color. You can move it onto text printed earlier
and it won't damage it: it remembers what was under it.

## Scrolling

When the screen is full, each new line slides everything up. `setScrollMode()` picks how:

- **`P486T_SCROLL_SMOOTH`** (the default) — the text glides up, a few pixels each
  frame, and speeds up when it falls behind.
- **`P486T_SCROLL_INSTANT`** — each line jumps into place. Faster when text pours in.

Scrolling never flashes or shows half-drawn lines: each new line is drawn out of sight
first, then scrolled in.

### When print() waits

`print()` normally returns at once. The terminal keeps eight lines of room below the
screen to draw into, so your sketch only waits when it gets more than eight lines
ahead of the scrolling. `flush()` waits until everything printed so far is on the
screen and the scrolling has stopped.

## A header and footer that stay put

`freeze(topPixels, bottomPixels)` takes strips at the top and bottom of the screen out
of the scrolling. Text scrolls between them, and you draw what you like into them with
`drawBitmap()`. `freeze(0, 0)` undoes it. What's on the screen stays where it is.
**TerminalHeaderFooter** shows how.

```cpp
term.freeze(60, 60);                                     // 60 px header and footer
term.drawBitmap(0, 0, 720, 60, headerPixels);            // into the header
term.drawBitmap(0, term.height() - 60, 720, 60, footerPixels);
```

`drawBitmap(x, y, w, h, pixels)` draws a picture at a position on the screen, counted in
pixels from the top left. `pixels` is `w × h` RGB565 colors, row by row — the format
most image-to-C converters produce for "RGB565". Parts off the screen are cut off.
It's fine to keep drawing into the frozen strips while text scrolls; a picture drawn
onto the scrolling part scrolls away with the text. At resolution 480, sizes passed to
`freeze()` are rounded up to even numbers. Make the header and footer multiples of 30
pixels (two lines here) and the text area stays a whole number of lines.

## Touch

```cpp
#include <P486Panel_Board.h>
#include <chipguy_P486Panel_terminal.h>

P486Panel_Board            board;
chipguy_P486Panel_display  display;
chipguy_P486Panel_touch    touch(board.Wire);    // touch is on the board's shared I²C bus
chipguy_P486Panel_terminal term;

// in setup(), after term.begin():
board.begin();
touch.begin();
term.attachTouch(touch);

// in loop():
int16_t col, row;
if (term.pollTouch(col, row)) {
    // the finger is on cell (col, row): same numbers as setCursor()
}
```

`pollTouch()` answers in character cells, so "was my button touched?" is a simple
comparison — see **TerminalTouchButtons**. Touches on a frozen header or footer don't
count in cells; ask for pixels instead with `term.pollTouch(x, y, P486T_TOUCH_PIXELS)`.
Rotation and resolution are already applied. The touch screen needs no calibration.

## Symbols and fonts

The built-in font has:

- English and the accented letters of Western, Central and Eastern European languages;
- Greek, with the accents of modern Greek;
- symbols common in projects: `° ± µ Ω ℃ ≤ ≥ ≈ √ ∞ ← ↑ → ↓ ✓ ✗ ⚠ ⚡ ★ ♥`, fractions,
  superscripts and subscripts (`CO₂`, `m²`), currency (`€ £ ¥`);
- **box drawing** (`┌─┐ │ └─┘ ═ ║ ╔ ╗`), drawn so lines meet exactly between cells,
  and block shapes (`█ ▌ ▐ ▀ ▄ ░ ▒ ▓`);
- **LVGL's symbols**, under LVGL's own names: `term.print(LV_SYMBOL_WIFI)` shows the
  same Wi-Fi icon an LVGL label would. **TerminalSymbols** shows them all.

The letters are drawn from **Inconsolata**, a free font in the style of Consolas; the
symbols from **DejaVu Sans Mono** and **Font Awesome Free**. A character the font
doesn't have prints as `�`.

**Japanese kana** come in a separate font you choose to include, about 66 KB:

```cpp
#include <p486t_font_mono16_ja_kana.h>
#include <chipguy_P486Panel_terminal.h>
// ...
term.begin(display, 0, 720, &p486t_font_mono16_ja_kana);
```

Hiragana and katakana take two cells; half-width katakana (ｶﾀｶﾅ) take one, like Latin
letters. Chinese, Japanese kanji and Korean fonts in the same format exist in the
`chipguy_ESP32-CYD-Resistive_display` library. They are 1–8 MB of source each, too
large to ship here; the font generator in this library's `extras/fontgen/` folder
builds them under this library's names.

## Good to know

- **The terminal owns the screen.** Once it's running, don't use LVGL or draw on the
  display yourself; draw pictures with `term.drawBitmap()`. `display.setBacklight()` is
  fine.
- **It uses one background task**, which scrolls, blinks the cursor and runs the
  graphics hardware. Printing from more than one of your own tasks is safe; each
  `print()` or `printf()` comes out whole.
- **Memory:** the screen and its eight spare lines live in PSRAM, of which the board
  has plenty: about 2.8 MB at 720 × 720 turned 0 or 180, and up to about 5 MB with the
  graphics hardware. Internal RAM: about 10 KB, all reserved when the sketch starts.
  The built-in font takes about 144 KB of flash.

---

## Where to go next

- Back to the [README](README.md) for LVGL and the rest of the board.
- [README-engineering.md](README-engineering.md#the-text-terminal) — how the terminal
  works inside: the display DMA's four-piece screen, the scroll ring, the graphics
  hardware path, and the measurements.
