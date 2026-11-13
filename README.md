# chipguy_P486Panel_display

A self-contained **display** driver for the **Waveshare ESP32-P4-86-Panel-ETH-2RO** —
the 86mm wall-panel board with a **720×720 MIPI-DSI LCD** and a **capacitive touch**
panel, on an ESP32-P4 with **32 MB flash** and **32 MB PSRAM**. It drives the panel so
you don't have to think about it: the hardware is fixed and fully handled, leaving you
to write an **LVGL 9** app and nothing else.

The panel is really two boards stacked together. The front board (sold on its own as the
"ESP32-P4-WIFI6-Touch-LCD-4B") has the screen, Wi-Fi, sound and the memory-card slot.
The back board adds a 12–24 V power input, two relays, an RS485 port and an Ethernet jack.
This library works with either; [the rest of the board](#the-rest-of-the-board) below
says what's where.

LVGL renders at the panel's **native 720×720**, so UI coordinates map 1:1 to pixels and
nothing is scaled. If you'd rather render at **480×480** and let the ESP32-P4's PPA scale
up — handy for sharing one UI across boards of different sizes — use the sibling library
[`chipguy_P486Panel_480display`](https://github.com/chipguyhere/P486Panel_480display).

### Two ways to drive the screen — pick one

- **LVGL** — buttons, sliders, screens, a full graphical app. Most of this README is about
  it, and so are the `Lvgl*` examples.
- **A text terminal** — `print()` and `printf()` onto a grid of characters that scrolls
  up the screen in hardware, like a computer's console. Good for a log, a status feed or
  a serial console, with no other library needed. It's in this library too:
  [README-terminal.md](README-terminal.md) and the `Terminal*` examples.

The bundled examples are the heart of the library. The two **starting points** are
complete and working — the display, touch, and LVGL are already wired up and running
before your code gets control. You pick the one that matches how you want to build your
UI, copy it, and start replacing its placeholder UI with your own. Two further examples,
**LvglCalculator** and **LvglBouncingBalls**, are finished apps you can read for ideas.

---

## Installation

This library is distributed by downloading the code as a ZIP from GitHub —
there is no Arduino Library Manager entry. To install:

1. On the GitHub project page, click **Code ▸ Download ZIP**.
2. Unzip it, and move the resulting folder into your Arduino **libraries**
   directory:
   - macOS / Linux: `~/Documents/Arduino/libraries/`
   - Windows: `Documents\Arduino\libraries\`
3. Restart Arduino IDE so the examples will appear in the File menu.

(Alternatively, in the IDE: **Sketch ▸ Include Library ▸ Add .ZIP Library…**
and select the downloaded ZIP.)

You also need the companion board library, **`chipguy_P486Panel_board`**
([GitHub](https://github.com/chipguyhere/P486Panel_board)), installed the same way. It owns the I²C bus the touch
screen shares with the sound chips, and it drives the relays. Touch itself is built into
this library; there is no separate touch or relay library any more.

### Dependency: LVGL 9

The examples need **LVGL 9**. Install it via the Arduino **Library Manager**
(search "lvgl"). Each example sketch folder ships its own `lv_conf.h`; LVGL reads
its configuration from that file, so keep it next to the `.ino`.

This library was developed against LVGL 9.3 (the current version of LVGL that is
compatible with SquareLine) but is very likely compatible with later versions.

### Arduino IDE board settings

This board uses a generic ESP32-P4 board definition. In **Tools**, set:

- **Board:** ESP32P4 Dev Module
- **PSRAM:** Enabled  *(required — the framebuffers live in PSRAM)*
  There is no size setting; "Enabled" is the whole choice, and the board's 32 MB is
  what you get.
- **Flash Size:** 32 MB (256 Mbit)  *(required — the bundled `partitions.csv` assumes it)*
- **Partition Scheme:** Custom  *(uses the `partitions.csv` in the example's folder)*
- **USB CDC On Boot:** depends on which USB-C port you plug into — see below.

If PSRAM or Flash Size is set wrong, the build stops with an error that says which one.
That is on purpose: a wrong setting would otherwise build fine and then crash on the
board. (A sketch that has to build for this board and a smaller-flash board from the same
source can switch the flash check off; the note at the top of `lv_setup_P486Panel.hpp`
explains how.)

#### Which USB-C port

The board has two USB-C ports on its edge:

- **Bottom port ("USB TO UART")** — the easy one. Leave **USB CDC On Boot: Disabled**.
- **Top port ("USB-OTG")** — the ESP32-P4's own USB. Set **USB CDC On Boot: Enabled**.

The examples print with **`Serial0`**, which always comes out of the **bottom** port. If
you're not sure, use the bottom port. The top port's setting costs some of the chip's
scarce internal memory; [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#the-two-usb-c-ports) has the
details.

---

## Rotation

Mounting the panel on its side is one line, before `lv_setup.begin()`. A popular choice is
**270**, which puts the USB ports and side buttons at the bottom, out of sight, and the
speaker holes at the top ([more on mounting](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#mounting-it-rotated)):

```cpp
display.setRotation(270);    // 0, 90, 180 or 270 (picture turned clockwise)
lv_setup.begin();
```

Touch is inverse-transformed automatically to match, so your UI coordinates stay in the
upright 720×720 space. Every example ships with this line present but commented out.

**It must come before `begin()`**, because at 720×720 there is nothing to scale and the
two orientations use genuinely different plumbing:

| | Buffers | Per-frame work |
|---|---|---|
| **rotation 0** | none extra — LVGL renders straight into the DMA framebuffers | cache sync + page flip; the PPA is never engaged |
| **90 / 180 / 270** | two 720×720 draw buffers (~1 MB each) | the PPA rotates each finished frame into the framebuffer |

So rotating costs about 2 MB of PSRAM and one PPA pass per frame here. (In the 480×480
sibling library the PPA is already scaling every frame, so rotation there is free and can
be changed at any time.)

---

## The examples

Open them from **File ▸ Examples ▸ chipguy_P486Panel_display**. All come up running on
the hardware immediately, so you can confirm your board works before you change a line.

To turn one into your own project, **save it under a new name** (File ▸ Save As) so your
work lives outside the library, then start editing. Each example folder is
self-contained — it carries its own `lv_setup.hpp`, `lv_conf.h` and `partitions.csv` —
so a copied sketch is fully standalone.

The `Terminal*` examples (TerminalHello, TerminalScrollSpeed, TerminalTouchButtons,
TerminalHeaderFooter, TerminalSymbols, TerminalMatrixRain) use the text terminal instead
of LVGL; [README-terminal.md](README-terminal.md#the-examples) describes them. The rest
of this section is about the LVGL ones.

### LvglClaudeCodeStub — write the UI yourself (by hand or with an AI)

Use this when you want to build the interface **in code**: directly, or by
handing the sketch to an AI coding assistant such as Claude Code.

The UI lives in a tiny `ui.h` / `ui.cpp` pair in the sketch folder. All the
hardware/LVGL setup happens in the sketch before `ui_init()` is called, so
`ui_init()` is a blank canvas: it receives a live LVGL screen and you create
widgets on it. To start your app:

1. Open `ui.cpp` and look at `ui_init()`. The demo it ships with is a **bring-up test** —
   a "Hello, world!" label that follows your finger, and a rectangle cycling
   RED ▸ GREEN ▸ BLUE once a second. Between them they prove touch alignment and panel
   color order (and the finger-follow is the quickest way to check a rotated build).
   Once you trust the hardware, **delete it.**
2. In its place, build your own UI on `lv_screen_active()` using normal LVGL
   calls (`lv_label_create`, `lv_button_create`, event callbacks, …).
3. As your app grows, add more screens and helper functions, and declare the
   ones the sketch needs in `ui.h`.

Because everything except the UI is already wired up, this scaffold is ideal to
hand to **Claude Code**: point it at the sketch folder and describe the app you
want — it can focus entirely on `ui.cpp` and the screens you add, without
touching display/touch/LVGL plumbing.

**Reach for this one** when you want full control, a tiny footprint, or
AI-assisted/hand-written UI code.

### LvglCalculator — a worked, resolution-independent example

A working four-function calculator styled after the dark iOS calculator. Its entire
layout — key diameter, gaps, margins, and font sizes — is derived from the live display
resolution at runtime, so nothing is pinned to 720×720. It's a good read for how to build
a UI in plain LVGL that travels between panels of different sizes. Not a blank starting
point — copy it if you want a calculator, or just study `build_keypad()` in `ui.cpp`.

### LvglBouncingBalls — physics and live controls

1–20 circular balls bounce under gravity, each a random color, re-launching to a random
height on every floor bounce. Two sliders at the bottom set the ball count and the speed.
Also fully resolution-independent, and a compact example of driving continuous animation
from `lv_timer_handler()` while touch input stays responsive.

### Lvgl93SquarelineLauncher — design the UI visually in SquareLine Studio

Use this when you'd rather **design the interface visually** in
[SquareLine Studio](https://squareline.io/) (a drag-and-drop LVGL UI editor) and
just run the result on the board.

The sketch's `src/` folder holds a small **placeholder** UI in the exact file
shape a SquareLine export produces (`ui.c`, `ui_Screen1.c`, …). The sketch calls
the `ui_init()` that SquareLine generates. To start your app:

1. In SquareLine Studio, create a project sized **720×720**, **16-bit** color,
   with **no rotation or offset**.
2. Lay out your screens visually.
3. **Export ▸ UI files** (the "Arduino TFT_eSPI profile" works, among others).
4. **Replace the whole `src/` folder** with your export. Build and upload —
   `ui_init()` now runs your design.

Treat `src/` as disposable: you overwrite it wholesale each time you re-export
from SquareLine, so don't keep anything there. Your own application logic lives
in the **sketch** — after calling `ui_init()`, attach event handlers from the
`.ino` to the UI objects SquareLine exposes (e.g. `ui_Button1`). That way
re-exporting the visuals never touches your code.

**Reach for this one** when you want to iterate on layout visually and write
little or no C for the UI itself.

---

## What the examples give you: the `lv_setup` API

Every example wires the hardware to LVGL through one small header, **`lv_setup.hpp`**
(plus **`lv_conf.h`** for LVGL's build config). That header, and the global objects it
defines, is the entire surface you interact with — and once `begin()` returns, you are
just using plain LVGL.

```cpp
#include "lv_conf.h"
#include "lvgl.h"
#include "lv_setup.hpp"

void setup() {
    Serial0.begin(115200);

    // display.setRotation(270);  // optional, must precede begin()
    lv_setup.begin();            // initialize display + touch + LVGL

    // ...build your UI on lv_screen_active() here (or call ui_init())...
}

void loop() {
    lv_timer_handler();          // let LVGL run
    delay(5);
}
```

What the header provides:

- **`lv_setup.begin()`** — call once in `setup()`. Brings up the DSI display, the
  capacitive touch, and LVGL; wires up the framebuffers, creates the LVGL display in
  **direct render mode** plus a pointer (touch) input device, and connects them. After
  this returns, the active LVGL screen exists and you build your UI with ordinary LVGL
  calls.
- **`board`** — a global `P486Panel_Board` from the board library: the shared I²C bus
  (`board.Wire`), the relays (`board.setRelay()`), and the board's pin numbers as named
  constants.
- **`display`** — a global `chipguy_P486Panel_display` object. Handy methods:
  `display.width()`, `display.height()` (both 720), `display.setRotation(deg)` /
  `display.getRotation()`, `display.isRotating()`, and `display.setBacklight(0..100)`.

You don't render or flush anything yourself — LVGL renders into the panel's framebuffers
and the driver flips them at vsync (tear-free, double buffered). Your job is to create
LVGL widgets and call `lv_timer_handler()` in `loop()`.

The example's `lv_setup.hpp` is a thin shim: a long comment plus a single `#include` of
the real implementation in this library's `src/`. Leave it alone and your sketch picks up
library improvements automatically; copy the library header's contents over that
`#include` when you need to take control — for multi-touch (LVGL consumes only one
contact point), or to share the framebuffer with another renderer. The shim's comments
walk through both.

### Portable across chipguy board libraries

Because every hardware detail lives in just two files — **`lv_setup.hpp`** and
**`lv_conf.h`** — your application code (`ui.cpp`, or the SquareLine `src/`
files) is hardware-independent. It only ever talks to LVGL.

So to move an app to a **different display board**, drop in the `lv_setup.hpp`
and `lv_conf.h` from that board's chipguy library, and the rest of your sketch
is unchanged. The same UI runs on different panels with no edits to your widget
code. (Different boards have different resolutions, so alignment-based layouts
travel best; fixed pixel coordinates may need adjusting — the calculator and
bouncing-balls examples show the resolution-independent style that travels cleanly.)

---

## The rest of the board

The screen is only part of this board. Here is everything else, in a few plain sentences
each, with a link to the full details. The details live in the board library,
`chipguy_P486Panel_board`, which you installed alongside this one.

### Touch

Touch is built into this library. You don't call it yourself: `lv_setup.begin()` starts
it. It can track one finger in LVGL, and up to five if you take over `lv_setup.hpp`.

### Relays — `chipguy_P486Panel_board`

The back board has **two relays** — switches the board can open and close, for things
like lights or a door strike. You can hear each one click. They start
**off**, and they turn off again if the board resets. `lv_setup.hpp` already gives you the
board object, called `board`:

```cpp
board.setRelay(0, true);    // relay 1 on
board.setRelay(0, false);   // relay 1 off
board.setRelay(1, true);    // relay 2 on
```

Which screw terminals are which, and what the relays can and can't switch:
[README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#relays).

### RS485

RS485 is a two-wire cable connection used by building and industrial equipment. The back
board has one on its power terminal block. It is a plain serial port to your sketch — no
library needed. The pins, and one wiring warning: [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#rs485).

### Ethernet

The board has a wired network jack. The Arduino ESP32 core already knows this board's
wiring, so `ETH.begin()` is all it takes: [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#ethernet).

### Wi-Fi and Bluetooth

A second chip, an ESP32-C6, handles Wi-Fi and Bluetooth. `WiFi.begin()` works as usual.
Boards shipped with old firmware on that chip. It works, but it's worth updating once with
a ready-made example from the Arduino ESP32 core: [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md#wi-fi-and-bluetooth)
says how.

### Sound — [README-sound.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-sound.md)

There is a plug for a small speaker, a sound chip, and two microphones. No library here
plays sound yet; the file explains how a sketch can, and what to watch for.

### TF / microSD card slot — [README-sdcard.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-sdcard.md)

There is a memory-card slot. This library doesn't use it, and it **shares nothing with
the display**, so the Arduino `SD_MMC` library works alongside it in either order.

### Power — [README-power.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-power.md)

The board runs from **12–24 V DC** on the back board's terminal, or from **either USB-C
port** — relays included. It has no battery and no power switch, and it can't turn
itself off.

### Pins, buttons and the expansion header — [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md)

Almost every pin is already in use. Seven are free, on a 14-pin header on the front board,
for adding your own sensors or buttons. BOOT and RESET are buttons on the side of the panel,
under a keycap that pulls off. The red light on the front is always on; it can be snapped
off if you don't want it.

**Don't put a coin cell in the 2-pin "RTC" connector.** The board charges whatever is
there, and an ordinary CR2032 is not made to be charged.

---

## Where to go next

| File | Who it's for |
|---|---|
| [`chipguy_P486Panel_board` README](https://github.com/chipguyhere/P486Panel_board/blob/main/README.md) | Everyone: the board library — relays, the shared I²C bus, the BoardScan example. |
| [README-board.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-board.md) | Developers: relays, RS485, Ethernet, the USB ports, the I²C bus, free pins, common hookups. |
| [README-power.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-power.md) | Developers: power inputs, what software can switch off, sleep and wake. |
| [README-sound.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-sound.md) | Developers: getting a beep, a sample or an MP3 out of the speaker, and recording. |
| [README-sdcard.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-sdcard.md) | Developers: mounting the card with the driver of your choice. |
| [README-limitations-errata.md](https://github.com/chipguyhere/P486Panel_board/blob/main/README-limitations-errata.md) | Everyone, before committing to the board: what it can't do or does wrong. |
| [README-terminal.md](README-terminal.md) | Everyone using the text terminal instead of LVGL: printing, colors, scrolling, touch, fonts. |
| [README-engineering.md](README-engineering.md) | Engineers: the display driver's internals — DSI bring-up, the warm-boot black panel, rotation, the terminal's hardware scrolling, the touch driver. |
| [Board engineering notes](https://github.com/chipguyhere/P486Panel_board/blob/main/README-engineering.md) | Engineers: the derivations behind the board's numbers. |
| [schematic-description.md](https://github.com/chipguyhere/P486Panel_board/blob/main/schematic-description.md) | Engineers, and LLMs without the drawing: both of Waveshare's schematics read out part by part, with the full GPIO map. |
| [`chipguy_P486Panel_480display`](https://github.com/chipguyhere/P486Panel_480display) | Anyone who wants to render at 480×480 instead. |

---

## License

This library is **mixed-license**:

- The original code (display driver, touch driver, text terminal, `lv_setup`, examples)
  is **MIT** — see [LICENSE](LICENSE).
- **`esp_lcd_panel_dpi_bb.c` / `.h`** is a fork of ESP-IDF's `esp_lcd_panel_dpi.c`
  (Copyright © Espressif Systems) with page-flipping and hardware-scrolling additions, and is licensed
  under the **Apache License, Version 2.0** — see
  [LICENSE-Apache-2.0.txt](LICENSE-Apache-2.0.txt). Those files carry SPDX headers
  recording both the upstream attribution and the modifications.

- **The terminal's fonts** (`p486t_font_mono16.cpp`, `p486t_font_mono16_ja_kana.h`) are
  glyphs rendered from other people's fonts and stay under those fonts' licenses:
  Inconsolata and Noto Sans CJK (SIL Open Font License 1.1), DejaVu Sans Mono (Bitstream
  Vera license) and Font Awesome Free 5.15.4 icons (CC BY 4.0, by Fonticons, Inc.). The
  license texts are in the `extras/fontgen/` folder, with the generator script. The
  generator and fonts come from `chipguy_ESP32-CYD-Resistive_display`.

The panel initialization/gamma values are standard register settings. The
TerminalHeaderFooter images were rendered by `extras/bitmaps/make_header_footer.py`, with
text set in DejaVu Sans.
Each example's `lv_conf.h` is based on LVGL's `lv_conf_template.h`, Copyright (c) LVGL Kft,
used under the MIT License (the same terms as [LICENSE](LICENSE)). The SquareLine example's
`src/` folder is SquareLine Studio output (a placeholder UI).

The GT911 touch driver (`chipguy_P486Panel_touch`) was rewritten in 2026 on the board
library's shared I²C bus. Copies of this library published before then carried a version
derived from Espressif's `esp_lcd_touch` / `esp_lcd_touch_gt911` (Apache-2.0) without its
license notice; that version is superseded and contains no code that remains here.
