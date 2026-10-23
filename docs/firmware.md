# DeskDisplay firmware

Custom ESP-IDF 5.5.2 / LVGL 9.3 project for the Waveshare ESP32-S3-RGB-Matrix
(ESP32-S3-N32R16) with **one 64×64 HUB75 panel**. Open this repository root in
VS Code. Hardware references are in `docs/hardware/`.

## Current firmware

- Large 12-hour clock, weekday, AM/PM, date, seconds indicator, and animated cyan ribbon.
- Five additional clock faces: cup, drain, hourglass, tide, and ripple.
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
All six faces, invalid/oversized command handling, framebuffer capture, and both
sides of the preview minute change passed on the board. The original clock
renderer was checked byte for byte against its previous version. RTC time
restored correctly after resets and firmware flashes. Preview did not alter UTC.
A performance check at each new face's rollover measured **27.9-29.1 rendered
frames/second**, with unchanged free heap (16,861,452 bytes including PSRAM).
These are short hardware checks; extended operation and RTC battery retention
still need longer testing.

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
.\.tools\host-python\Scripts\python.exe scripts/device.py mode cup
.\.tools\host-python\Scripts\python.exe scripts/device.py mode drain
.\.tools\host-python\Scripts\python.exe scripts/device.py mode hourglass
.\.tools\host-python\Scripts\python.exe scripts/device.py mode tide
.\.tools\host-python\Scripts\python.exe scripts/device.py mode ripple
.\.tools\host-python\Scripts\python.exe scripts/device.py mode clock
.\.tools\host-python\Scripts\python.exe scripts/device.py preview drain
.\.tools\host-python\Scripts\python.exe scripts/device.py preview off
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

- Short press **BOOT**: clock → cup → drain → hourglass → tide → ripple → clock.
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
| `main/ui/desk_ui.c` | Original clock, LVGL canvas, page selection, minute timing, previews |
| `main/ui/clock_faces.c` | Five water faces, pixel graphics, and digit transitions |
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

## Clock faces and minute timing

| Mode | Progress over the minute | Minute change |
| --- | --- | --- |
| `clock` | Original ribbon and seconds bar | Original immediate digit update |
| `cup` | A stream fills a glass | Glass tips and spills; new digits drip into place |
| `drain` | Water rises across the whole background | A vortex pulls in the old digits and releases the new time |
| `hourglass` | Water transfers between two glass reservoirs | The glass and digits flip to begin another minute |
| `tide` | Tide rises around a lighthouse; its beam sweeps once | A breaking wave washes the next minute onto the display |
| `ripple` | Second drops make ripples and light 60 perimeter marks | A large circular ripple reveals the next minute |

New faces start their three-second transition when the real clock crosses a
minute boundary. The previous and current local times drive the two digit layers,
including hour/day changes. Progress uses fractional wall-clock seconds, so
dropped frames do not accumulate drift. Clock syncs, backward time jumps, startup,
and switching faces do not replay a false rollover. New faces show the upcoming
time as the water reveals it; the original face changes immediately as before.

`preview <face>` compresses the filling stage into ten seconds, then plays the
normal three-second rollover and briefly shows the next fill. The header reads
`PREVIEW`; live time resumes after 16 seconds or `preview off`. Preview does not
write the system clock or RTC. The original face is excluded from preview.

For a repeatable visual sample, use `preview drain --sample-ms 61500` in the USB
helper. Samples 0..59999 cover filling, 60000..62999 cover rollover, and
63000..64999 show the next minute. A frozen sample expires after 30 seconds.
`verify` checks all six faces, both sides of each preview rollover, invalid
commands, frame progress, and that preview leaves real UTC unchanged.

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
- [Six-face overview](images/clock-faces.png) and [animated comparison](images/clock-faces.gif)
- Individual previews: [cup](images/cup.gif), [drain](images/drain.gif),
  [hourglass](images/hourglass.gif), [lighthouse tide](images/tide.gif), and
  [ripple pool](images/ripple.gif)

The previews use frames rendered on the connected board. Filling is compressed
to ten seconds for comparison; the three-second rollover retains its normal timing.

Hardware and API references:
[Waveshare example source](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix),
[Waveshare ESP-IDF guide](https://docs.waveshare.net/ESP32-S3-RGB-Matrix/ESP-IDF/),
[HUB75 driver](https://components.espressif.com/components/esphome/esp-hub75/versions/0.3.5/readme),
[LVGL display setup](https://docs.lvgl.io/9.3/details/main-modules/display/setup.html).
