# chipguy_P486Panel_display — engineering notes

The display driver's internals, for someone who reads schematics and datasheets: the
MIPI-DSI bring-up, the warm-boot black-panel errata and its fix, the rotation path, the
text terminal's hardware scrolling, and the GT911 touch driver. The 480×480 sibling has its own short
[engineering file](https://github.com/chipguyhere/P486Panel_480display/blob/main/README-engineering.md)
for what differs there.

The rest of the board (pins, relays, RS485, Ethernet, USB, power, sound, the TF slot)
is documented in the board library, `chipguy_P486Panel_board`, and derived in its
[engineering notes](https://github.com/chipguyhere/P486Panel_board/blob/main/README-engineering.md). The § references below are to the complete
schematic reading, [schematic-description.md](https://github.com/chipguyhere/P486Panel_board/blob/main/schematic-description.md), which also
lives there.

Contents: [Display](#display) · [The text terminal](#the-text-terminal) ·
[Touch](#touch-gt911) · [What was not derived](#what-was-not-derived)

---

## Display

### MIPI-DSI bring-up

- **PHY supply:** `esp_ldo` channel 3 (VO3) at **2500 mV** — the D-PHY's requirement,
  shared with CSI.
- **Bus:** 2 data lanes, **600 Mbps per lane**, PHY PLL from `PLL_F20M`.
- **DPI:** RGB565, **46 MHz** pixel clock. Timing: HSYNC 20 / HBP 80 / HFP 80, VSYNC 4 /
  VBP 12 / VFP 30. H_total = 720 + 180 = 900 and V_total = 720 + 46 = 766, so
  46 MHz / (900 × 766) = **66.7 Hz** refresh.
- **Link budget:** 46 MHz × 16 bpp = 736 Mbps of pixel payload against 2 × 600 = 1200 Mbps
  of lane capacity, before DSI packet overhead and blanking-period LP transitions. That is
  61 % utilization, with headroom.
- **Framebuffers:** two 720×720×2 = 1,036,800-byte buffers in PSRAM (taller for the
  terminal, below), owned by the forked DPI driver `esp_lcd_panel_dpi_bb.c`. It rebuilds
  the DMA descriptor list in the end-of-frame interrupt for every frame, which gives
  tear-free page flipping (`chipguy_lcd_dpi_panel_set_active_fb`) without the IDF
  driver's copy-on-draw, and the terminal's scan layout (below).
- **Panel:** ST7703-class controller (§1.4). Init sequence `B9 F1 12 83` (extension
  command unlock) … gamma `E0`, then `11` sleep-out (250 ms) and `29` display-on (50 ms).
  Reset is released after the DSI link is up, followed by the ST7703's ≥ 120 ms settle,
  then the init sequence over DBI-on-DSI (LP mode).

### The warm-boot black panel, and the two mechanisms against it

The driver comments carry the full bench history; this is the summary.

**Symptom.** Intermittently, after a **soft reset** (OTA, `esp_restart()`, panic, task/int
WDT), the panel comes up black. The backlight is on, firmware runs, LVGL renders, the DPI
engine scans pixels at full rate, and the panel answers DCS reads claiming sleep-out and
display-on. Every host-side observation matches a working boot. Cold boots never show it.

**Why only after a soft reset.** A soft reset is CPU-only (rst:0x0C). The DSI PHY, its LDO
and the panel all stay powered and mid-stream. The panel's only reset input is GPIO27,
which reverts to input during the reset. The panel is never power-cycled; its supply
(`ESP_3V3`) has no switch.

**What was measured** (natural failures, not induced):

| Attempt | Result |
|---|---|
| light sleep with TOP + CNNT powered down, plus a full DSI rebuild | no |
| deep sleep (digital core and PHY powered down) | no |
| RWDT system reset (rst:0x10) | recovered once, then failed twice on the same unit |
| EN button (whole-chip reset) | reliably recovers |
| RESETn pulse: 100 ms low, 100 ms high, **released to input**, 100 ms quiet, before anything else | recovers many units, but not all — suspected PCB revision difference |
| Super-watchdog reset (rst:0x12, "digital core and rtc module") from a **running** link | 20/20 recovered (480display, 2026-08-18) |
| the same reset **after** an orderly DSI teardown | 0/11 |

**What the driver does**, in `begin()`:

1. **Recovery pulse first** (`_recoveryResetPulse`), before any DSI setup. The *release*
   — leaving RESETn undriven for 100 ms — is the step that mattered. The code comment
   says RESETn is "held by the board's own pull-up". The schematic shows **no pull-up on
   the main board**: `RESET` connects only to R60 and the LCD connector (§1.4). So the
   level during the release is set by the panel module, either a pull-up on the FPC or
   the controller's own input. **TODO (bench):** measure the RESET line with GPIO27 as an
   input.
2. **Warm-boot promotion** (`setWarmBootQuiesce`, on by default). If the reset reason is
   in the soft-reset family, bring the display up normally. Then, at the *bottom* of
   `begin()` with the link owned and running, arm the **super watchdog** (clear
   `swd_auto_feed_en` and `swd_disable`, stop feeding; it fires in about 1.8 s). The next
   boot reports `ESP_RST_WDT`, which is not in the promote list. An NVS flag
   (`cgp486/promoted`) is a second loop guard, set before promoting and cleared on the
   next boot. RTC RAM can't hold that flag, because these resets are exactly what wipe it.
   Cost: one extra ~1.5–2 s boot cycle per soft reset, nothing on a cold boot.

The promotion's placement is itself a measured result. At the top of `begin()` the DSI
hardware is still running from the previous image with no software owning it, and
promoting from there did not help. A teardown before the promoted reset also defeats it
(0/11), so the code calls `_triggerDeepestReset()` with the link still running.

**The two display libraries run the same boot-time recovery.** The trials were run with
the 480×480 sibling; its bring-up up to the promotion point is the same as this one's.
The probe and manual-recovery tools used during the investigation, and the teardown
path, were removed from both libraries once the fix was settled.

`resetAndRestart(hold_ms)` is the caller's escape hatch for a failed `begin()`: backlight
off, RESETn held low for `hold_ms` (default 10 s), then `esp_restart()`, with reset held
asserted across the restart.

### Rotation: the PPA path

At rotation 0 LVGL renders in DIRECT mode straight into the two DMA framebuffers. The flush
is `esp_cache_msync` (C2M) plus a page flip at vsync, and the PPA is never engaged. A
rotation can't be done in place: for 90/180/270, `begin()` allocates two more 720×720
buffers (128-byte aligned — the P4's PSRAM cache line; 64-byte alignment produces "not
aligned with cache line size" on every msync) and registers a PPA SRM client. Each flush
then becomes C2M on the draw buffer → `ppa_do_scale_rotate_mirror` at scale 1.0 into the
back framebuffer (blocking) → M2C on the target → flip. Square panel, so 90/270 need no
resize. Cost: 2 × 1,036,800 bytes of PSRAM and one full-frame PPA SRM pass per flush.
The PPA reads and writes PSRAM over the same bus the DPI engine is scanning out of. At
66.7 Hz the DPI already reads 69 MB/s, and a full-frame rotate adds about 2 MB per frame
read+written. `getUnderrunCount()` is the counter to watch if a sketch combines rotation
with heavy PSRAM traffic of its own.

---

## The text terminal

`chipguy_P486Panel_terminal` is the CYD terminal's front end (printing, `printf`, the
fonts, the API) over a back end written for this panel. Measured on the bench unit,
2026-11-13, core 3.3.11.

**Cell size.** The CYD's fonts are 16 × 32, which gives 22½ lines on a 720-pixel screen.
This library's are regenerated at **16 × 30** (baseline 25) by the same script from the
same sources, which reproduces the CYD's 16 × 32 output byte for byte when run at that
size. 30 divides both 720 (24 lines) and 480 (16). Of the built-in font's 1,396 glyphs, 48
had ink in the top or bottom row that 30 drops: tilde capitals such as Ã and Ñ, and
commas, cedillas and some Greek descenders. The script shifts each of those by one
pixel to fit. Only U+222B ∫ and U+0123 ģ are taller than 30 pixels in all, and lose a
pixel. Box drawing and block elements are drawn to the cell, so they still meet.

### The scan layout: four regions in six blocks

The display DMA (the DW-GDMA, feeding the DSI bridge) reads each frame through a linked
list of blocks, and the forked driver rebuilds that list in the end-of-frame interrupt
before restarting the channel. So any change to *where* a frame is read from takes effect
at a frame boundary and cannot tear. `chipguy_lcd_dpi_panel_set_layout()` describes a
frame as up to four regions of framebuffer rows: a frozen top band, `window` rows taken
from a ring of `ring` rows starting `offset` rows in (two regions when the window wraps
round the ring's end), and a frozen bottom band. Scrolling is a change of `offset` alone.

The block count is **fixed at six**. `mipi_dsi_brg_ll_set_multi_block_number()` tells the
bridge, once at creation, how many blocks make up a frame, so a list that varied between
two and four blocks with the layout would disagree with it. The driver instead always
emits six: the natural regions, with the largest split in half until there are six. That
also keeps every block at 360 rows or fewer, the size the original two-block layout ran
at. The block-size register itself (`block_ts`, 22 bits of 64-bit items) is no limit. A
row is 1440 bytes, a multiple of the DMA's 16-byte alignment, so any row boundary is a
legal block start. The plain layout — what LVGL uses — is one region of 720 rows, split
into six blocks of 120. LVGL ran unchanged on it in both display libraries.

### Direct mode: the framebuffer is the ring

At 720 × 720 and rotation 0, the terminal asks the display for a framebuffer of
**720 + 240 rows** (`setFramebufferRows()`) and lays it out as [top band][ring][bottom
band], the ring being the scrolling window plus `P486T_SPARE_LINES` × 30 = 240 spare
rows. Text is drawn straight into it, the cache is written back
(`esp_cache_msync`, C2M) for the rows touched, and the frame task sets the scan offset.
No pixel is copied to scroll.

Rotation 180 is the same layout stored upside down: [bottom][ring][top], every row and
every pixel reversed, with the window running through the ring backwards
(offset = −(vtop + window) mod ring). The DMA still reads forwards.

### The ring, and the one-frame lag

Positions in the scrolling area are virtual rows; virtual row *v* lives in ring row
*v* mod *R*. The spare rows exist so a new line is cleared and drawn **before** it scrolls
in, so scrolling never shows a half-drawn row. That is the CYD terminal's "fast vs
clean" trade-off, which doesn't arise here. Two rules keep it correct:

- The frame task never scrolls the window past rows that have not been cleared.
- A row is only cleared once its alias one ring-length earlier, *v* − *R*, is above the
  window the display is **actually scanning**.

The second rule first used the window the task had just *asked for*. In direct mode the
DMA picks a new offset up at the next frame boundary, so for one frame the panel still
showed the old window. Clearing against the new one blanked ring rows that were on
screen for that frame, seen as black missing scanlines in the text near the top of the
screen while it scrolled (reported 2026-11-13). The terminal now tracks the window as
applied (`_vtopLive`, the offset set one task iteration earlier) and clears against the
lower of the two. The extra frame of lag halved the instant-scroll burst rate:

| 720, rotation 0 | smooth | instant |
|---|---|---|
| before the fix (glitching; 32-pixel lines) | 125 lines/s | 530 lines/s |
| after (30-pixel lines) | 100 lines/s | 267 lines/s |

Smooth scrolling's rate is also a deliberate cap: it moves at most a quarter of the
backlog per frame, so text visibly scrolls rather than jumping.

`P486T_SPARE_LINES` bounds how far printing may run ahead of the scrolling; doubling it
would win the instant rate back for about 0.35 MB more PSRAM per framebuffer.

### PPA mode: rotation 90/270 and the 480 resolution

The DMA can only move the picture along the panel's native rows, so a terminal turned
90/270 — or stretched to 480 → 720 — needs the PPA. The terminal keeps an upright
surface in PSRAM laid out [top][ring][ring again][bottom]. The ring is stored **twice**,
back to back, so any window of it is one contiguous block: no split at the wrap point,
which at a 1.5× scale would leave a half-pixel seam. For the same reason freeze heights
are rounded up to even at 480. Each update is at most three PPA scale-rotate blits
(header, window, footer) into the back framebuffer, then a page flip. Updates run only
when something changed (text, the cursor's blink, a scroll step), at most once per frame.

Measured PPA time per update (full 720 × 720 output, blocking mode):

| Mode | Per update | 300-line burst |
|---|---|---|
| 480, rotation 0 or 270 (scale 1.5) | 12.0 ms | 52 printed lines/s = 104 screen lines/s (33-character lines wrap at 30 columns) |
| 720, rotation 90 or 270 (scale 1, rotate) | 21.1–21.2 ms | 53 lines/s |

(After the one-frame-lag fix, with 30-pixel lines. Before it, the same modes measured
the same per-update times and 112 and 64 lines/s.)

A 720 × 720 rotation takes longer than one 15 ms frame. So while scrolling, the turned
720 terminal updates every other frame, and the lines-per-second rate halves against
direct mode. It doesn't tear: the flip is requested after the blits and applied at a
frame boundary, and the task waits for that boundary before writing the other buffer.
The LVGL rotation path in this library does the same full-frame rotate per flush, so it
presumably pays the same per frame (not measured in LVGL itself). A faster design
would keep the turned terminal's text stored pre-rotated, so the PPA only copies;
not done.

### The cursor

Over text the terminal no longer holds, the CYD terminal has to read pixels back from
the panel to blink the cursor. Here the whole screen is in memory: the cursor saves the
pixels it covers (up to a double-width cell, 32 × 32) and puts them back. It never
damages old text, and needs no readback check.

---

## Touch (GT911)

`chipguy_P486Panel_touch` (in this library's `src/`) drives a GT911 at **0x5D** on the
board's shared I²C bus (§1.4, §1.7).

- **It uses the board's bus.** The touch driver takes `board.Wire`, the board library's
  mutex-wrapped `Wire` on GPIO7/8. `lv_setup` calls `board.begin()` before
  `touch.begin()`. Each register access is one locked transaction: a 16-bit register
  address, a repeated start, then the drain of `Wire`'s RX buffer under the same `Guard`.
  The codec, the microphone ADC and user devices can share the bus from other tasks
  ([why it is shared this way](https://github.com/chipguyhere/P486Panel_board/blob/main/README-engineering.md#the-shared-i2c-bus)).
- **No interrupt.** INT goes to TP2 only, so touch is polled once per LVGL input period.
  The GT911's address-select-by-INT sequence can't run, so the address is whatever the
  controller latches with INT untouched: 0x5D on every board so far, including the bench
  unit on 2026-11-13 (product ID "911", config version 70). `begin()` also tries 0x14,
  the GT911's other address.
- **Reset first.** `begin()` pulses RESETn (GPIO23) low for 10 ms before its first
  transaction. After a watchdog reset the GT911 has not been power-cycled and can be
  holding SDA low from a half-finished transaction, which wedges the bus for every device
  on it.
- **Retry.** Up to 20 attempts 250 ms apart to read the product ID. After a soft reboot the
  GT911 can take several attempts to answer.
- **Reading.** Status register 0x814E: bit 7 means a new report is ready, and the low 4
  bits give the contact count. Contacts follow from 0x814F, 8 bytes each, and the status
  is cleared once a report has been read. Between reports (the GT911 reports roughly every
  10 ms while touched) the driver returns the **last report again**. A touch ends with
  the controller's zero-contact report, or after 100 ms with none. The earlier driver
  reported "no touch" whenever no new report was ready, which LVGL never noticed at its
  30 ms input period. But a terminal sketch that called `pollTouch()` twice in a row saw
  every second call come back released.
- **Coordinates** arrive in native 720×720 space. `lv_setup` applies the inverse rotation
  only.
- **Five touch points** are read (`TOUCH_MAX_POINTS`, `readPoints()`); LVGL's pointer
  indev consumes the first.
- **Provenance.** The first version of this driver was Espressif's `esp_lcd_touch` +
  `esp_lcd_touch_gt911` (Apache-2.0), vendored without its notices. The current driver is
  a rewrite on `ThreadsafeWire`, with the register sequence taken from the GT911's
  programming guide, and contains none of that code.

---

## What was not derived

- Panel and touch controller identities are inferred from the code, not the sheet. The
  GT911's product ID ("911") was read on the bench.
- No current, voltage or timing was measured for this file. The bench results quoted
  under [Display](#display) are the driver's own field history.
- Datasheet figures recalled rather than checked are listed in the board library's
  engineering notes, where they are used.
