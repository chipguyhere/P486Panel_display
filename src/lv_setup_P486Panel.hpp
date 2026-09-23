/*
 * lv_setup_P486Panel.hpp - Connects LVGL to the P486 Panel display and touch,
 * at any rotation.
 *
 * This is the *actual* LVGL-to-driver glue for this board.  It ships inside the
 * library (src/) so every example can share one copy.  Each example's sketch
 * folder contains a small lv_setup.hpp that simply #includes this file; that
 * thin shim is the file you edit if you want to take this code over and
 * customize it (see the comments in any example's lv_setup.hpp).
 *
 * Include this file (or, normally, the example's lv_setup.hpp) after lvgl.h.
 *
 * ROTATION
 *   One code path covers every orientation.  Call
 *
 *       display.setRotation(0 | 90 | 180 | 270);
 *
 *   in your sketch BEFORE lv_setup.begin(); omit it and you get 0.
 *
 *   Unlike the 480x480 sibling library, this one renders at the panel's native
 *   720x720, so there is nothing to scale and the two orientations use
 *   genuinely different plumbing:
 *
 *     rotation 0    LVGL renders straight into the DMA framebuffers and the
 *                   flush is just a cache sync plus a page flip.  The PPA is
 *                   never engaged and no extra PSRAM is spent.
 *
 *     rotation 90   Rotation cannot be done in place, so begin() allocates two
 *      / 180 / 270  720x720 draw buffers (about 1 MB each) for LVGL and the
 *                   flush hands each finished frame to the PPA, which rotates
 *                   it into the DMA framebuffer.
 *
 *   Because that choice decides what begin() allocates, setRotation() has to be
 *   called before it.  Both flush paths live in _disp_flush() below, selected
 *   once by display.isRotating().
 *
 * GEOMETRY
 *   LVGL renders 720x720 - the panel's native resolution, so UI coordinates map
 *   1:1 to pixels at every rotation.  LVGL runs in DIRECT render mode with two
 *   buffers.  Touch arrives in native 720x720 space and needs no scaling, only
 *   the inverse rotation transform.
 */

#pragma once

#ifdef CHIPGUY_LV_SETUP_INCLUDED
#error "Another lv_setup_*.hpp is already included - it also defines `display` and `lv_setup`."
#endif
#define CHIPGUY_LV_SETUP_INCLUDED

// This board has 32MB flash (and 32MB PSRAM); the bundled partitions.csv is laid out
// for it.  The IDE exposes no numeric flash-size macro, but it encodes the menu
// selection in ARDUINO_FQBN.  Fail at compile time if Flash Size isn't 32MB
// (Tools > Flash Size); a smaller flash yields a boot-looping image, because the
// bundled table puts app1 and the data partitions past the end of the smaller
// flash the build was configured for.
//
// ESCAPE HATCH: define CHIPGUY_ALLOW_ANY_FLASH_SIZE to waive the check.
//
// It is here for the sketch that drives SEVERAL different boards from one source
// tree and therefore has to name a single Flash Size for all of them - a 16M
// build runs perfectly well on this board's 32MB part, it simply leaves the top
// half unused.  Waiving the check hands you the partition table: supply one that
// fits the smaller size (a sketch-local partitions.csv overrides whatever
// PartitionScheme names) and keep the app slots big enough for OTA.
//
// Single-board sketches, and every example in this library, should leave the
// check alone.  Defaulting to 32M is what provisions the generous OTA slots the
// bundled table assumes, and for a project that only ever targets this board
// there is nothing to gain by giving that up.
//
// Note the check only guards the DOWNWARD direction.  Configuring a flash size
// LARGER than the physical part is the one that bricks a board into a boot
// loop, and the second-stage bootloader catches that itself at startup with
// "Detected size(...) smaller than the size in the binary image header(...)".
#if defined(ARDUINO_FQBN) && !defined(CHIPGUY_ALLOW_ANY_FLASH_SIZE)
namespace {
    constexpr bool _cg_fqbn_contains(const char *hay, const char *needle) {
        for (const char *h = hay; *h; ++h) {
            const char *a = h, *b = needle;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (!*b) return true;
        }
        return false;
    }
}
static_assert(_cg_fqbn_contains(ARDUINO_FQBN, "FlashSize=32M"),
    "chipguy_P486Panel_display requires Flash Size = 32MB. Set Tools > Flash Size in the Arduino IDE, "
    "or define CHIPGUY_ALLOW_ANY_FLASH_SIZE if this sketch deliberately targets a smaller one.");
#endif

#include <chipguy_P486Panel_display.h>
#include <chipguy_P486Panel_touch.h>
#include "esp_cache.h"

static chipguy_P486Panel_display display;

class lv_setup_class {
public:
    // Optional callback invoked on every touch press (e.g. activity timeout).
    void (*onTouch)() = nullptr;

    // Initializes display, touch input, and LVGL.
    // Call display.setRotation() before this if rotation is desired.
    void begin() {
        // WARNING: display.begin() MAY NOT RETURN -- it can reboot the board,
        // once, and that is deliberate.
        //
        // This panel has an errata: after a WARM restart -- ESP.restart(), an
        // OTA reboot, a panic, or a software watchdog reset -- it can come up
        // persistently BLACK.  Backlight on, firmware running, LVGL rendering,
        // the DPI engine scanning real pixels out at full rate, the panel
        // reporting sleep-out and display-on: every host-side measurement reads
        // identical to a working boot, only the picture is missing.  A soft
        // reset is a CPU-only reset (rst:0x0C) and leaves whatever latches the
        // fault untouched; measured in the field, only a reset that takes the
        // RTC domain with it clears the panel reliably.
        //
        // So on a warm boot the driver brings the DSI link up properly, then
        // promotes the soft reset to a Super Watchdog reset (system + RTC), so
        // the LCD is forced through a full reset along with the chip.  The boot
        // after that sees a watchdog reset reason, skips the promotion, and
        // brings the display up for real.
        //
        // Consequences for the sketch:
        //   - Anything setup() did BEFORE this call runs a second time after
        //     the reboot, so nothing here may be a must-happen-exactly-once
        //     side effect.
        //   - A warm restart costs one extra ~1.5 s boot cycle.  A cold boot
        //     (power-on or the EN button) pays nothing -- no promotion.
        //   - It cannot loop: both the reset reason and an NVS flag bound it to
        //     at most one promotion per soft reset.
        //
        // display.setWarmBootQuiesce(false), called before this, opts out and
        // leaves only the cheaper RESETn recovery pulse at the top of begin() --
        // which clears some black panels but, measured, not all of them.
        if (!display.begin()) {
            // Don't strand the board in a dead loop: hold the panel in reset
            // for 10 s and reboot, so an init failure retries by itself.
            Serial0.println("Display init failed! Resetting panel and rebooting in 10s...");
            Serial0.flush();
            display.resetAndRestart(10000);  // does not return
        }
        Serial0.println("Display initialized");

        _touchscreen.begin();

        // Which buffers LVGL draws into depends on the mode begin() chose.
        // Rotating: its own draw buffers, which the PPA then rotates into the
        // framebuffer.  Not rotating: the DMA framebuffers themselves, so the
        // renderer's output needs no copy at all.
        _rotating = display.isRotating();
        if (_rotating) {
            _buf0 = display.getDrawBuffer(0);
            _buf1 = display.getDrawBuffer(1);
        } else {
            _buf0 = display.getFramebuffer(0);
            _buf1 = display.getFramebuffer(1);
        }
        _buf_size = display.framebufferSize();

        lv_init();

        static lv_disp_t *disp = lv_display_create(display.width(), display.height());
        lv_display_set_flush_cb(disp, _disp_flush);
        lv_display_set_buffers(disp, _buf0, _buf1, _buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_user_data(disp, this);

        static lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, _touchpad_read);
        lv_indev_set_user_data(indev, this);

        lv_tick_set_cb(_tick_get);
    }

private:
    chipguy_P486Panel_touch _touchscreen;
    uint16_t *_buf0 = nullptr;
    uint16_t *_buf1 = nullptr;
    size_t _buf_size = 0;
    bool _rotating = false;
    uint8_t _target_fb_index = 0;

    static uint32_t _tick_get(void) { return millis(); }

    static void _disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap) {
        auto *self = (lv_setup_class *)lv_display_get_user_data(disp);
        if (lv_display_flush_is_last(disp)) {
            uint8_t buf_index = ((void *)pixelmap == (void *)self->_buf0) ? 0 : 1;

            if (self->_rotating) {
                // PPA rotates the finished 720x720 frame into the next DMA
                // framebuffer and activates it.
                display.rotateAndFlip(buf_index, self->_target_fb_index);
                self->_target_fb_index ^= 1;
            } else {
                // LVGL rendered directly into the DMA framebuffer -- no copy,
                // just push the CPU's writes out to PSRAM and flip to it.
                esp_cache_msync(pixelmap, self->_buf_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
                display.setActiveFramebuffer(buf_index, false);
            }
        }
        lv_display_flush_ready(disp);
    }

    static void _touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
        auto *self = (lv_setup_class *)lv_indev_get_user_data(indev);
        esp_lcd_touch_handle_t tp = self->_touchscreen.getHandle();
        if (!tp) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        uint16_t x[TOUCH_MAX_POINTS], y[TOUCH_MAX_POINTS];
        uint16_t strength[TOUCH_MAX_POINTS];
        uint8_t cnt = 0;

        esp_lcd_touch_read_data(tp);
        bool pressed = esp_lcd_touch_get_coordinates(tp, x, y, strength, &cnt, TOUCH_MAX_POINTS);

        if (pressed && cnt > 0) {
            // Touch is already in native 720x720 space, the same space LVGL
            // renders in, so there is no scaling -- only the inverse of
            // whatever rotation the PPA applied to the image.
            int32_t tx = (int32_t)x[0];
            int32_t ty = (int32_t)y[0];

            switch (display.getRotation()) {
                case 90:
                    // PPA rotated the image 90 degrees CCW; undo that for touch.
                    // Inverse touch transform: sx = 719 - ty, sy = tx
                    data->point.x = 719 - ty;
                    data->point.y = tx;
                    break;
                case 180:
                    data->point.x = 719 - tx;
                    data->point.y = 719 - ty;
                    break;
                case 270:
                    data->point.x = ty;
                    data->point.y = 719 - tx;
                    break;
                default: // 0 - no rotation
                    data->point.x = tx;
                    data->point.y = ty;
                    break;
            }
            data->state = LV_INDEV_STATE_PRESSED;
            if (self->onTouch) self->onTouch();
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
};

static lv_setup_class lv_setup;
