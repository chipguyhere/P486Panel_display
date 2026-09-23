/*
 * ESP32-P4 DSI Display driver with double-buffering support
 * For 720x720 MIPI DSI display
 */

#pragma once

#ifdef ARDUINO_ESP32P4_DEV

// This display driver allocates its framebuffers in PSRAM.  The board carries
// 32 MB of PSRAM (and 32 MB of flash), so there is ample room -- but it must
// be switched on: the Arduino IDE defines BOARD_HAS_PSRAM when PSRAM is
// enabled (Tools > PSRAM: "Enabled").
// Fail loudly at compile time rather than crashing at runtime if it's off.
#ifndef BOARD_HAS_PSRAM
#error "chipguy_P486Panel_display requires PSRAM. Enable it via Tools > PSRAM: \"Enabled\" in the Arduino IDE."
#endif

// The Flash Size check that used to live here now sits in
// lv_setup_P486Panel.hpp, next to the include guard.  It belongs there because
// this header is compiled into the library's own .cpp as well, where a sketch's
// #define cannot reach it - putting the check in a header only ever included BY a
// sketch is what lets CHIPGUY_ALLOW_ANY_FLASH_SIZE be set the obvious way.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"   // the DSI PHY supply, kept so it can be released
#include "driver/ppa.h"


class chipguy_P486Panel_display {
public:
    chipguy_P486Panel_display();

    // Initialize the display hardware
    bool begin();

    // Warm-boot reset promotion (on by default; see the long note in the .cpp).
    //
    // A soft reset is a CPU-only reset (rst:0x0C), which leaves whatever latches
    // this panel's black-screen fault untouched -- measured: only a reset that
    // includes the RTC domain clears it.  When enabled, a boot that followed a
    // soft reset brings the DSI up, shuts it down cleanly, then promotes itself
    // to an RWDT system+RTC reset; the boot after that runs normally.
    //
    // Costs one extra ~1.5 s boot cycle per soft reset and nothing on a cold
    // boot.  Guarded against boot loops by both the reset reason and an NVS
    // flag.  Call before begin() to disable.
    // Runs in addition to the RESETn recovery pulse at the top of begin(), which
    // clears some black panels on its own but not all of them; this is the
    // fallback for the rest.  Disabling it leaves only the pulse.
    void setWarmBootQuiesce(bool enable) { _quiesce_on_warm_boot = enable; }


    // Recovery path for a failed begin().  Holds the panel in hardware reset
    // for hold_ms -- far longer than the 20 ms assert used during normal
    // bring-up -- to let it fully quiesce, then restarts the ESP32 so the
    // whole init sequence runs again from a clean state.  Does not return.
    [[noreturn]] void resetAndRestart(uint32_t hold_ms = 10000);

    // Get framebuffer by index (0 or 1)
    uint16_t *getFramebuffer(uint8_t index);

    // Set which framebuffer the display hardware shows
    void setActiveFramebuffer(uint8_t index, bool wait_for_vsync = true);

    // --- Rotation -----------------------------------------------------------
    // This panel is native 720x720 and LVGL renders at 720x720, so there is no
    // scaling to do: at rotation 0 the renderer draws straight into the DMA
    // framebuffer and the PPA is never touched.  Rotation is the one thing that
    // cannot be done in place, so asking for 90/180/270 switches begin() into a
    // second mode: it allocates two 720x720 PSRAM draw buffers (about 1 MB each)
    // for the renderer and uses the PPA to rotate each finished frame into the
    // DMA framebuffer.
    //
    // Because that choice decides what begin() allocates, CALL setRotation()
    // BEFORE begin().  Afterwards you may freely change among angles only if
    // begin() already put you in the rotating mode; asking to start rotating
    // after a rotation-0 begin() is rejected and logged, since the draw buffers
    // do not exist.  Accepts 0, 90, 180, 270 (counter-clockwise).
    void setRotation(int degrees);
    int  getRotation() const;

    // True when the PPA path is active, i.e. begin() allocated draw buffers
    // because rotation was non-zero.  Decides which buffers the renderer should
    // draw into and which flush path it should use.
    bool isRotating() const;

    // Buffers the renderer should draw into when isRotating().  Returns nullptr
    // when not rotating -- in that mode draw straight into getFramebuffer().
    uint16_t *getDrawBuffer(uint8_t index);

    // PPA-rotate the given draw buffer into the specified DMA framebuffer, then
    // activate it.  Only meaningful when isRotating().
    void rotateAndFlip(uint8_t draw_buf_index, uint8_t target_fb_index);

    // Wait for vsync (frame boundary)
    bool waitVsync(uint32_t timeout_ms = 50);

    // Get number of framebuffers
    uint8_t getNumFramebuffers();

    // Convenience getters
    int16_t width() const { return 720; }
    int16_t height() const { return 720; }
    size_t framebufferSize() const { return (size_t)720 * 720 * sizeof(uint16_t); }

    // Backlight control (0 = off, 100 = full brightness)
    void setBacklight(int percentage);

    // Debug counters
    uint32_t getFrameCount();
    uint32_t getUnderrunCount();

private:
    // Drive RESETn low 100 ms, high 100 ms, release the pin (input, no pull),
    // then 100 ms of quiet -- the sequence measured to recover panels that stay
    // black even through an EN-pin reset.  Called first thing in begin(), before
    // any other hardware is touched.  The normal bring-up assert still runs
    // afterwards, unchanged; this only has to happen first.
    void _recoveryResetPulse();
    bool _shouldPromoteWarmBootReset();
    void _shutdownDsi();
    void _triggerDeepestReset();

    // Serialises the flush path against _shutdownDsi(); see scaleAndFlip().
    SemaphoreHandle_t _dsi_mutex = nullptr;
    bool _quiesce_on_warm_boot = true;
    bool _promote_pending = false;
    esp_ldo_channel_handle_t _ldo_mipi_phy = nullptr;
    esp_lcd_dsi_bus_handle_t _mipi_dsi_bus = nullptr;
    esp_lcd_panel_handle_t _panel_handle = nullptr;
    esp_lcd_panel_io_handle_t _io_handle = nullptr;
    ppa_client_handle_t _ppa_client = nullptr;
    uint16_t *_draw_buffers[2] = {nullptr, nullptr};  // Two 720x720 PSRAM buffers, only when rotating
    ppa_srm_rotation_angle_t _rotation = PPA_SRM_ROTATION_ANGLE_0;
    int _rotation_degrees = 0;
    bool _began = false;
};

#endif // ARDUINO_ESP32P4_DEV
