# DeskDisplay firmware

Custom ESP-IDF 5.5.2 / LVGL 9.3 project for the Waveshare ESP32-S3-RGB-Matrix
(ESP32-S3-N32R16) with **one 64×64 HUB75 panel**. Open this repository root in
VS Code. Hardware references are in `docs/hardware/`.

## Current firmware

- Large 12-hour clock, weekday, AM/PM, date, seconds indicator, and animated cyan ribbon.
- One additional island face: compact date, rounded time with inline AM/PM, palms,
  a small white sailboat on a subtle cyan ribbon, and two visiting pixel birds.
- Double-buffered HUB75 DMA with complete LVGL frames to avoid stale pixels.
- ES8311 speaker test: a short two-note chime at 65% codec volume, with fade-in/out.
  The PCM signal is also attenuated. `sound 0..85` adjusts the test volume.
- PCF85063 RTC stores UTC; display uses America/New_York rules with daylight saving.
- USB commands and a live framebuffer capture for troubleshooting.
- Starts on Island at 25% panel brightness (an explicitly saved custom scene can
  still take precedence). Brightness resets on reboot.
- Wi-Fi/SNTP acquires internet time on connection and every 3,600 seconds, with
  reconnect attempts after network loss. Successful syncs update the UTC RTC.

Calendar, weather, and timer integrations remain future firmware work. Wi-Fi is
used for internet time; no OAuth or microphone recording is involved.

## Wi-Fi credentials and hourly internet time

Put your 2.4 GHz network credentials in `wifi.local.json` in the repository root:

```json
{"ssid":"YOUR_WIFI_NAME","password":"YOUR_WIFI_PASSWORD"}
```

`wifi.example.json` is the shareable template; `wifi.local.json` is ignored by Git.
Credentials are sent over USB and stored in the board's NVS, rather than compiled
into the firmware. Close the serial monitor, then run:

```powershell
.\.tools\host-python\Scripts\python.exe scripts/device.py --port COM4 wifi wifi.local.json
.\.tools\host-python\Scripts\python.exe scripts/device.py --port COM4 network-check
.\.tools\host-python\Scripts\python.exe scripts/device.py --port COM4 timezone-check
```

After changing networks, edit the same file and rerun `wifi`; rebuilding/flashing
is not required. SSIDs support 1–32 UTF-8 bytes; WPA passwords use 8–63 printable
ASCII characters. An empty password is allowed for an open network.

Three NTP Pool servers provide UTC. Sync starts when Wi-Fi gets an IP address and
repeats hourly (`ntp_interval_s=3600`). `status` reports Wi-Fi state, the last
successful NTP epoch, sync count, New York local time, EST/EDT, and UTC offset.
If the network is unavailable, the clock continues from the UTC RTC/system clock.

New York uses `EST5EDT,M3.2.0,M11.1.0`: EST in winter and EDT in summer. The on-board
`timezone-check` tests both spring-forward and fall-back conversions without
changing the real clock. The underlying timezone/SNTP APIs follow the
[ESP-IDF system-time documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/system_time.html).

## Verified on the connected board (2026-10-23)

Additional verification on **2026-10-28**: the bootloader, partition table, and
Wi-Fi-enabled application were flashed to COM4 with hash verification. Island
started by default. Credentials were provisioned from the ignored local JSON;
the board acquired NTP time, reported `wifi=connected`, `clock=set`,
`ntp_interval_s=3600`, and New York EDT (`-0400`). After a software reboot,
credentials persisted, Island restarted, and a fresh NTP sync succeeded. All
four on-board spring/fall DST boundary cases passed. Existing mode/bounds/frame
progress/audio-initialization checks passed. A framebuffer capture is in
`artifacts/island-internet-time.png`. The hourly interval was checked directly;
this was not a one-hour endurance test.

ESP-IDF build and flash passed, including flash hash verification. The user
confirmed the physical clock display and audible chime at codec volume 65.
The original clock renderer was checked byte for byte against its previous
version. RTC time restored correctly after resets and firmware flashes.
Both clock faces, removal of the old modes, command bounds, and framebuffer
progress passed on the connected board. A 65-second island run verified bird
departure and return, a live minute change, unchanged UTC progression, and stable
free heap (16,869,412 bytes including PSRAM), averaging 28.6 rendered frames/second.
The observed flight interval was 26.9 seconds. The island face uses live time
directly, with immediate minute updates. These are short hardware checks;
extended operation and RTC battery retention still need longer testing.

## VS Code

Use **Terminal → Run Build Task** (`Ctrl+Shift+B`) for `DeskDisplay: Build`.
Other tasks under **Terminal → Run Task**:

- `DeskDisplay: Flash COM4`
- `DeskDisplay: Monitor COM4` (exit with Ctrl+])
- `DeskDisplay: Sync clock`
- `DeskDisplay: Speaker test`

Close the serial monitor before another task needs COM4. The ESP-IDF extension
can stay installed; these tasks supply the local compiler environment themselves.
IntelliSense uses `build/compile_commands.json` after a successful build.

Run these PowerShell commands from the repository root:

```powershell
.\scripts\idf.ps1 build
.\scripts\idf.ps1 -p COM4 flash
.\scripts\idf.ps1 -p COM4 monitor
.\.tools\host-python\Scripts\python.exe scripts/device.py sync
.\.tools\host-python\Scripts\python.exe scripts/device.py status
.\.tools\host-python\Scripts\python.exe scripts/device.py mode island
.\.tools\host-python\Scripts\python.exe scripts/device.py mode clock
.\.tools\host-python\Scripts\python.exe scripts/device.py brightness 25
.\.tools\host-python\Scripts\python.exe scripts/device.py sound
.\.tools\host-python\Scripts\python.exe scripts/device.py sound 75
.\.tools\host-python\Scripts\python.exe scripts/device.py test
.\.tools\host-python\Scripts\python.exe scripts/device.py capture artifacts/clock.png
.\.tools\host-python\Scripts\python.exe scripts/device.py verify
```

`sync` copies the PC's current UTC time into the system clock and RTC. With a
working RTC backup battery, time survives removal of main power. Without a valid
RTC time the original clock shows `SET TIME`; the island face shows dashes.
Internet time is synchronized automatically when Wi-Fi is configured. The serial helper auto-detects a single Espressif
USB device; use `--port COM4` before the command when multiple devices are present.

## On the board

- Short press **BOOT**: clock -> island -> clock.
- Hold **BOOT** for one second: play the speaker test once.
- **RST**: restart; defaults to Island at 25% brightness, or an uploaded custom scene.
- `test`: an eight-second red/green/blue/gray test with asymmetric white corner
  marks, then returns to the clock. This checks channel order, row addressing,
  orientation, and clipping.

Keep the panel's normal external 5 V supply connected. USB programming does not
replace the LED panel's power requirements. If the ROM loader is needed, hold
BOOT, tap RST, release BOOT, and run the flash task again.

## Layout and extension points

| File | Responsibility |
| --- | --- |
| `main/board/display.cpp` | LVGL task, RGB565 flush, HUB75 DMA, framebuffer readback |
| `main/ui/desk_ui.c` | Original clock, LVGL canvas, and page selection |
| `main/ui/island_face.c` | Minimal island scene, small typography, boat, and bird timing |
| `main/board/clock.c` | Shared I²C bus, RTC, UTC clock, display timezone |
| `main/board/audio.c` | I²S / ES8311 and queued sound playback |
| `main/main.c` | Startup, debounced button, USB commands, health reporting |
| `scripts/device.py` | Clock sync, controls, capture, integration checks |
| `sdkconfig.defaults` | Reproducible board/panel/flash/PSRAM defaults |
| `dependencies.lock` | Resolved component versions; keep in version control |

All external UI changes acquire `lvgl_port_lock()`. Animation callbacks run in
the LVGL task. Sounds are queued to a separate task so I²S writes do not stall
animation. Future calendar/weather providers should send bounded events to the
application task, which can select screens and queue sounds. Keep network requests,
JSON parsing, and OAuth refresh out of LVGL callbacks.

The flash layout includes a 4 MB factory application, NVS/PHY, and a 1 MB scene
partition with two slots for interrupted-upload recovery. Remaining flash is
reserved; OTA updates are not configured. Wi-Fi credentials use a separate NVS
namespace and normal firmware flashes preserve them.

## Clock faces

`clock` preserves the original weekday/AM/PM header, large time, date, animated
cyan ribbon, and seconds bar. Island is now the default startup face.

`island` places a small date such as `Oct 23` at the top. Rounded 12-hour digits
and AM/PM share a line; no weekday appears. A small sand island has two coconut
palms, and a white-sailed boat bobs on the ribbon as a rigid sprite. Permanent
water starts at the island shoreline; the ribbon highlights it with a four-pixel
total vertical range (two pixels in each direction). Trunks and fronds are thicker.
All boat pixels share one integer vertical offset, with no rotation. The backdrop
uses off pixels. The entire scene below the clock is translated down eight pixels,
with water spanning all 64 columns and three independently phased wave ribbons.
The boat is drawn after every ribbon so its pixels remain in front. The time changes
immediately at each minute, without a transition or full-screen animation.

Two tiny birds start perched near the palms. A randomly chosen interval of
20–30 seconds starts each departure or return, with a short staggered flight.
A hardware-seeded PRNG and monotonic clock control their timing; adjusting the
RTC does not speed up the animation. Bird state and time until the next flight
appear in USB `status` as `birds` and `birds_next_ms` for inspection. The scene
uses the existing RGB565 canvas without allocating per-frame image buffers.

`verify` exercises both modes, rejects removed modes and invalid commands,
and checks display frame progress and audio initialization. Live framebuffer
captures come from the board through the same USB connection.

## Local tools

Tools are project-local under `.tools/`: ESP-IDF checkout, Espressif compilers,
IDF Python environment, and a separate host flashing/serial environment. Build
output, downloaded dependencies, generated captures, and tools are ignored by Git
and hidden in the VS Code Explorer. Source configuration and `dependencies.lock`
remain versioned.

On a fresh checkout, install Git and Python 3.11, then run
`.\scripts\setup.ps1`. It downloads ESP-IDF 5.5.2, the ESP32-S3 tools, and Python
dependencies into this folder. Pass `-Python C:\path\to\python.exe` if needed.

Normal flashes rewrite the project firmware regions without erasing the entire chip.

## Hardware references and screenshots

- [Board schematic](hardware/schematic.pdf)
- [Board dimensions](hardware/dimensions.pdf)
- [Original clock](images/clock.png)
- [Island face](images/island.png) and [subtle motion capture](images/island.gif)

Hardware and API references:
[Waveshare example source](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix),
[Waveshare ESP-IDF guide](https://docs.waveshare.net/ESP32-S3-RGB-Matrix/ESP-IDF/),
[HUB75 driver](https://components.espressif.com/components/esphome/esp-hub75/versions/0.3.5/readme),
[LVGL display setup](https://docs.lvgl.io/9.3/details/main-modules/display/setup.html).
