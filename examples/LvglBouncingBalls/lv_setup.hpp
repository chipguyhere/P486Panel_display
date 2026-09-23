/*
 * lv_setup.hpp - The bridge between LVGL and your display + touch hardware.
 *
 * THIS is the file that wires LVGL up to a specific board.  Right now it just
 * pulls in the ready-made setup that ships inside the
 * chipguy_P486Panel_display library, so you don't have to maintain a copy:
 *
 *   - It creates a global `display`     (the 720x720 MIPI-DSI panel driver)
 *   - It creates a global `lv_setup`    (an lv_setup_class instance)
 *   - Calling lv_setup.begin()          initializes display + touch + LVGL
 *
 * --------------------------------------------------------------------------
 *  GEOMETRY
 *  LVGL renders at the panel's native 720x720, so UI coordinates map 1:1 to
 *  pixels and touch needs no scaling.  If you want a 480x480 canvas scaled up
 *  to this panel instead - handy for sharing a UI with smaller boards - use the
 *  sibling library chipguy_P486Panel_480display.
 *
 *  ROTATION - one line in the sketch
 *  There is no separate "rotated" version of this file to swap in.  The setup
 *  handles all four orientations; to rotate, uncomment the setRotation() line
 *  in the .ino, before lv_setup.begin():
 *
 *      display.setRotation(90);   // 0, 90, 180 or 270
 *
 *  Unlike the 480 sibling - where the PPA is always busy scaling anyway and
 *  rotation rides along for free - here rotation genuinely costs something,
 *  because at 720x720 there is nothing to scale:
 *
 *    rotation 0    LVGL renders straight into the DMA framebuffers.  The PPA
 *                  is never engaged and no extra PSRAM is used.
 *    rotated       Rotation can't be done in place, so begin() allocates two
 *                  720x720 draw buffers (~1 MB each) and the PPA rotates each
 *                  finished frame into the framebuffer.
 *
 *  That's why setRotation() must come BEFORE lv_setup.begin() - it decides what
 *  begin() allocates.  Touch is inverse-transformed automatically either way.
 *
 *  TARGETING A DIFFERENT BOARD
 *  Point the #include below at a different library's setup header and the same
 *  sketch (ui.cpp / ui.h, etc.) drives a different board - as long as that
 *  header exposes the same `display` / `lv_setup` shape.
 * --------------------------------------------------------------------------
 *
 *  CUSTOMIZING HOW LVGL IS WIRED UP
 *  Because this file lives in your sketch folder, it's the easy place to take
 *  control.  Open the real implementation at:
 *
 *      <Arduino libraries>/chipguy_P486Panel_display/src/lv_setup_P486Panel.hpp
 *
 *  copy its entire contents over the #include line below, and edit freely.
 *  Nothing else in the sketch needs to change.  Things that need a fork:
 *
 *    - MULTI-TOUCH.  LVGL's pointer input device consumes exactly one contact
 *      point.  _touchpad_read already reads up to TOUCH_MAX_POINTS contacts
 *      from the controller, then hands LVGL only x[0]/y[0] and discards the
 *      rest.  Fork this file to route those extra contacts into your own
 *      pinch / rotate / two-finger handling while still feeding point 0 to
 *      LVGL.  Each extra contact needs the same inverse rotation transform
 *      applied.
 *
 *    - SHARING THE FRAMEBUFFER.  If LVGL should drive the panel only part of
 *      the time - a video decoder, a camera preview, or your own direct-draw
 *      code takes over for a while - fork this file to gate _disp_flush and
 *      control which DMA framebuffer gets activated, so the other renderer can
 *      own the panel between LVGL sessions.  At rotation 0 this is especially
 *      direct: LVGL is already drawing into the framebuffer itself, so the
 *      other renderer can simply write to the other one and flip.
 *
 *    - Buffer count and render mode (LV_DISPLAY_RENDER_MODE_DIRECT vs
 *      PARTIAL); the color format; the touch mapping or deadzone; the LVGL
 *      tick source (millis() vs esp_timer); or the init-failure policy
 *      (currently: hold the panel in reset for 10 s, then reboot and retry).
 *
 *  THE TRADEOFF: leave this file alone and your sketch picks up library fixes
 *  and improvements automatically.  A forked copy is frozen at the moment you
 *  copied it, and becomes yours to maintain.
 *
 * Include this file after lv_conf.h and lvgl.h in your sketch.
 *
 *
 * For reference, the setup class this pulls in looks like:
 *
 *   // A global instance named `lv_setup` is created for you.
 *   class lv_setup_class {
 *   public:
 *       // Optional callback invoked on every touch press, e.g. to reset an
 *       // activity/screensaver timer.  Fires on each poll while a touch is
 *       // held, so it can repeat during a single press.
 *       void (*onTouch)();
 *
 *       // Initialize display, touch, and LVGL.  Call once from setup().
 *       // Call display.setRotation() before this if rotation is desired.
 *       void begin();
 *   };
 *
 *   // Also provided as a global: the display driver:
 *   //   display.width(), display.height()          // both 720
 *   //   display.setRotation(degrees), display.getRotation()
 *   //   display.isRotating()                       // true if on the PPA path
 *   extern chipguy_P486Panel_display display;
 *
 * Note: this panel uses capacitive touch - no calibration step is needed.
 */

#pragma once

#include <lv_setup_P486Panel.hpp>
