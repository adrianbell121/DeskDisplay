# DeskDisplay firmware

Custom ESP-IDF 5.5.2 / LVGL 9.3 project for the Waveshare ESP32-S3-RGB-Matrix
(ESP32-S3-N32R16) with **one 64×64 HUB75 panel**. Open this repository root in
VS Code. Hardware references are in `docs/hardware/`.

## First version

- Large 12-hour clock, weekday, AM/PM, date, seconds indicator, and animated cyan ribbon.
- Orbit and green pixel-rain animations at a target of 30 frames per second.
- Double-buffered HUB75 DMA with complete LVGL frames to avoid stale pixels.
- ES8311 speaker test: a short two-note chime at 65% codec volume, with fade-in/out.
  The PCM signal is also attenuated. `sound 0..85` adjusts the test volume.
- PCF85063 RTC stores UTC; display uses America/New_York rules with daylight saving.
- USB commands and a live framebuffer capture for troubleshooting.
- Starts at 25% panel brightness. Modes and brightness reset to defaults on reboot.

Calendar, weather, network time, and timer triggers are future work. This version
uses no Wi-Fi credentials, OAuth tokens, microphone recording, or cloud services.

## Verified on the connected board (2026-10-23)

ESP-IDF build and flash passed, including flash hash verification. The user
confirmed the physical clock display and audible chime at codec volume 65.
Clock, orbit, rain, invalid/oversized command handling, and live framebuffer
capture passed. RTC time restored correctly after a reset and another firmware
flash. A subsequent 20-second check measured **28.6 rendered frames/second** with
unchanged free heap (16,869,708 bytes including PSRAM). This is a short hardware
check; extended operation and RTC battery retention still need longer testing.

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
.\.tools\host-python\Scripts\python.exe scripts/device.py mode orbit
.\.tools\host-python\Scripts\python.exe scripts/device.py mode rain
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
RTC time the UI explicitly shows `SET TIME`, rather than an invented date.
Network time can be added later. The serial helper auto-detects a single Espressif
USB device; use `--port COM4` before the command when multiple devices are present.

## On the board

- Short press **BOOT**: clock → orbit → rain → clock.
- Hold **BOOT** for one second: play the speaker test once.
- **RST**: restart; defaults to clock at 25% brightness.
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
| `main/ui/desk_ui.c` | Pixel graphics, labels, animation timer, screen state |
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

Only a 4 MB factory application partition and NVS/PHY partitions are defined.
Remaining flash is reserved. OTA and asset partitions need an explicit design
before introducing remote firmware updates or large animation libraries.

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
- [Clock](images/clock.png), [orbit](images/orbit.png), and [rain](images/rain.png) captures

Hardware and API references:
[Waveshare example source](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix),
[Waveshare ESP-IDF guide](https://docs.waveshare.net/ESP32-S3-RGB-Matrix/ESP-IDF/),
[HUB75 driver](https://components.espressif.com/components/esphome/esp-hub75/versions/0.3.5/readme),
[LVGL display setup](https://docs.lvgl.io/9.3/details/main-modules/display/setup.html).
