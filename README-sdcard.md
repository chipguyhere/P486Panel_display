# Waveshare ESP32-P4-86-Panel-ETH-2RO — TF / microSD card slot

Technical notes for the card slot, referenced from the `chipguy_P486Panel_display` and
`chipguy_P486Panel_480display` READMEs (same board, same slot — this is the one copy).

The board has a microSD (TF) slot on a **4-bit SDMMC** interface, on the ESP32-P4's
dedicated SDMMC IOMUX pins. Neither display library drives it and none of the family
implements a filesystem; the pinout is here so a sketch can bring up whatever driver it
prefers.

## Pinout

| Slot pin | GPIO | Notes |
|---|---|---|
| CLK | **GPIO43** | |
| CMD | **GPIO44** | |
| D0 | **GPIO39** | |
| D1 | **GPIO40** | |
| D2 | **GPIO41** | |
| D3 | **GPIO42** | CS if you ever ran it in SPI mode — but don't, see below |
| CD (switch) | — | not routed — no card-detect |

Source: Waveshare's wiki for the board, which gives exactly this `sdmmc_slot_config_t`
(`width = 4`, `clk = 43`, `cmd = 44`, `d0..d3 = 39..42`). These are the P4's **SDMMC
slot 0 IOMUX pins**, so the card runs on the dedicated pads rather than the GPIO
matrix — the same wiring Espressif uses on its own P4 boards.

**Pull-ups.** Waveshare's demo sets `SDMMC_SLOT_FLAG_INTERNAL_PULLUP`, which suggests
the board does not fit external ones; the P4's internal pull-ups are enough for
SDMMC at the speeds below. **TODO (schematic):** confirm from
`86_Panel_Bottom_Board.pdf`.

**Card power.** The ESP32-P4 can feed a card's IO rail from an on-chip LDO (that's how
Espressif's dev boards do it, and the Arduino core's `SD_MMC.setPowerChannel()` exists
for it). Waveshare's demo configures **no** LDO, and the core's `SD_MMC` treats "no
power channel" as "externally powered", so on this board the rail is evidently a
fixed 3.3 V supply and nothing needs to be set. It also means the card runs at
**3.3 V signaling only**: default speed (20 MHz) or high speed (40 MHz) — no UHS-I
1.8 V modes, whatever "SDIO 3.0" in the product copy suggests. **TODO (schematic):**
confirm the card VDD source.

## Bus sharing

**Not known to be shared.** GPIO39–44 appear nowhere else in the family or in the
board's documented pin map: the display is MIPI-DSI (dedicated lanes; reset GPIO27,
backlight GPIO26), touch is I²C on GPIO7/8 with reset GPIO23, the relays are GPIO32
and GPIO46, and Ethernet is RMII on GPIO28–31/34/35/49–52.

## SPI / SDMMC hosts already in use

| Host | Used by | For |
|---|---|---|
| SDMMC slot 0 | — | **free — and it's where the card is** |
| SDMMC slot 1 | — | free |
| `SPI2_HOST` / `SPI3_HOST` | — | free; neither display library nor the touch or relay libraries use SPI |
| MIPI-DSI | `chipguy_P486Panel_display` / `_480display` | the panel |
| SPI0/SPI1 | ESP-IDF | flash and PSRAM — never touch |

Use the **SDMMC host, 4-bit**. Driving this slot in SPI mode would throw away the
dedicated interface for no gain.

## Starting points

```cpp
// SDMMC 4-bit — Arduino core. Works before or after lv_setup.begin(); nothing shared.
#include <SD_MMC.h>
SD_MMC.setPins(/*clk*/ 43, /*cmd*/ 44, /*d0*/ 39, /*d1*/ 40, /*d2*/ 41, /*d3*/ 42);
bool ok = SD_MMC.begin("/sdcard", /*mode1bit=*/ false);   // 4-bit, 20 MHz default
// For 40 MHz: SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_HIGHSPEED);
```

```cpp
// ESP-IDF VFS mount — what Waveshare's demo does
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;           // optional, 40 MHz
sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
slot.width = 4;
slot.clk = GPIO_NUM_43; slot.cmd = GPIO_NUM_44;
slot.d0 = GPIO_NUM_39; slot.d1 = GPIO_NUM_40; slot.d2 = GPIO_NUM_41; slot.d3 = GPIO_NUM_42;
slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
esp_vfs_fat_sdmmc_mount_config_t mnt = { .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024 };
sdmmc_card_t *card;
esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mnt, &card);
```

Neither is a supported API of these libraries; they are the calls that match the
wiring.
