# DeskDisplay Widget Studio

A local visual designer for a 64×64 panel. Design and animate widgets, save a
library, create clock faces and variations, and preview data bindings without a board.

## Launch

From the repository root:

```powershell
.\scripts\preview.ps1
```

Open <http://127.0.0.1:8765> in any browser on this computer, or double-click
Launch Dashboard.cmd / the desktop shortcut. All browsers load the same shared
workspace from data/workspace.json. Faces, widget library, variations, default
face and preview settings are shared. Export workspace creates a portable backup.
The server is required; opening index.html directly is no longer supported.
The server binds to loopback and exposes only the preview folder plus its APIs.
It does not open, enumerate, probe, or flash a board on startup.

## Designer and animations

- Drag widgets onto the canvas or click Add. Select to move, resize, recolor,
  duplicate, hide, lock, reorder, or remove them. Arrow keys nudge; Shift moves
  eight pixels. Ctrl+Z and Ctrl+Shift+Z undo/redo layout changes.
- Keyframe animation offers Float, Slide, Pop, and Fade presets. Set duration,
  looping, and Linear/Smooth/Step interpolation, then add keys at the playhead.
  Keys control X/Y offsets relative to the base position, scale, and opacity.
  Up to 32 keys and a 1–16 second loop are supported. **Save to widget library**
  retains the track for reuse. **Save variation** retains the whole face.
- Draw sprite animations in the Pixel Workshop. Paint/erase, undo/redo strokes,
  duplicate/add/remove/reorder frames, preview playback, set FPS, import an image,
  and show the previous frame as a faint ghost. Sprites support 1–64 pixels per
  dimension, 15 colors plus transparency, and up to 32 frames.
- **Bake face to animated sprite** captures the whole composition at the sample
  time/data. Review its frames before saving. This freezes the clock/date and
  provider values; it is an explicit snapshot, not live data.

## Library page

Browse **Clock faces**, **Widgets**, or **Animations**, with search and sorting.
Open faces, create variations, place widgets, edit saved sprite pixels, export,
or delete with Undo available. Editing a library animation updates that saved
entry; copies already placed on faces stay independent.

The three starter layouts—Weather today, Five-day outlook, Daily agenda—are
editable mockups. **Use editable starter** adds a face to your collection.
The catalog is upgraded with new widget types without replacing saved faces.

## Date formatting

Select a Date widget, or bind Text to `clock.date`, and set **Date format**:

| Pattern | Example |
| --- | --- |
| `MM-DD-YYYY` | `10-28-2026` |
| `YYYY/MM/DD` | `2026/10/28` |
| `ddd, MMM D` | `Wed, Oct 28` |
| `[DATE:] DD.MM.YY` | `DATE: 28.10.26` |
| `hh:mm A` | `02:05 PM` |

Tokens: `YYYY YY`, `MMMM MMM MM M`, `DD D`, `dddd ddd`, `HH H hh h`,
`mm m ss s`, `A a`. Uppercase `MM` means month; lowercase `mm` means minute.
Literal words belong in square brackets; punctuation passes through unchanged.
**Fit text width** sets native pixel width up to 64. Longer strings can be
compressed by resizing; a 64-pixel panel still imposes readability limits.
The compact font supports Latin letters, digits, and common punctuation.
Computer time follows the browser timezone; sample time is deterministic.

## Data bindings and mock widgets

Select an element's **Function / source** in its inspector. Data stays separate
from layout; editing **Data & bindings** updates every element using that source.
Edited sample data is validated, saved in the workspace, and included in exports.

| Widget/source | Supported preview content |
| --- | --- |
| `weather.current` | Sun/cloud/rain/snow icon, temperature in °F, precipitation probability |
| `weather.forecast` | Five daily icons and temperatures; continuous probability curve across hourly samples |
| `weather.tempF`, `weather.precipProbability` | Bind a Text widget with a prefix/suffix |
| `calendar.events` | Two or three events today/tomorrow, titles, start times, all-day labels |
| `clock.date` | Local clock with the selected date-format pattern |

Weather is **sample data**. Google Calendar is **not connected**. The calendar
adapter accepts Events.list-shaped records for preview, including timed/all-day
events, excludes cancelled/out-of-window events, and normalizes titles/IDs.
The adapter follows the documented [Google Calendar event resource](https://developers.google.com/workspace/calendar/api/v3/reference/events)
fields for `summary`, `status`, and `start.date`/`start.dateTime`.
OAuth, recurring-event expansion, timezone conversion into a chosen display zone,
scheduled refresh, and automatic live weather fetching remain future integrations.
No credentials, account access, or location are requested by these mockups.

The curve smoothly interpolates probability samples within 0–100%; it does not
represent precipitation amount or claim more forecast resolution than the input.

Backend routes (all local):

- `GET /api/providers`: capabilities and truthful connection statuses.
- `GET /api/data?date=YYYY-MM-DD`: deterministic sample payload.
- `POST /api/data/preview`: validate canonical weather/calendar values.
- `POST /api/calendar/preview`: normalize `{items, referenceDate}`.

POSTs require the matching loopback Origin, JSON, and `X-Face-Lab: 1`. The data
contract has `referenceDate`, `current`, exactly five consecutive `days`,
2–240 ordered `hourly` samples, and up to 100 `events`. Temperatures are °F;
precipitation probabilities are percentages. Provider records are currently
included in the shared workspace; the server validates them without connecting a calendar account.

## AI

**Ask AI** requests a reusable widget, new face, or variation through the locally
installed Codex CLI and its sign-in. Each request uses account usage and runs only
when Generate is clicked. Results are bounded JSON data, not executed JavaScript,
and are reviewed before adding to the studio. Generation is read-only and does
not upload to the board. CLI sign-in/network availability must be configured by
the user; tests use a mock model process. Alternatively, copy the request into
this chat and paste/import the JSON response.

## Deployment boundary

The existing USB scene player supports sprites plus its built-in live clock,
`MMM D` date, and seconds track. Studio explicitly rejects unsupported live
provider bindings, custom date patterns, or unbaked keyframe tracks during
package compilation. It never silently uploads stale sample data as live weather
or calendar information. Use an explicitly baked sprite snapshot for those
designs, or extend the firmware/provider transport in a later change.

USB begins only after **Upload selected face** is clicked. With hardware available,
the scene player requires a one-time firmware build/flash, documented in the Deploy
dialog. The Island board-layout upload has been verified on the connected board,
including pixel comparison, moving/resizing an element, and persistence after restart.

## Verification and source layout

```powershell
node --test preview/engine.test.cjs preview/workspace.test.cjs preview/scene.test.cjs preview/designer.test.cjs preview/island-assets.test.cjs
python -m unittest discover -s scripts -p test_*.py
```

`scene.js` validates/renders editable widgets; `animation.js` interpolates tracks;
`widget-data.js` handles date formatting and bindings; `studio-state.js` migrates
and merges workspaces; `app.js` manages canvas/sprite editing; `designer.js`
implements Library, Data, and inspector extensions. `widget_providers.py` defines
backend contracts. Tests exercise formats, leap dates, interpolation, bounds,
rendering, persistence, migration, HTTP validation, and mocked USB/AI.


## Original Island assets

Open **Island — board layout** to edit the current native Island design. Its twelve
independent layers are also in the widget library: clock, date, permanent sea,
three wave ribbons, shore, two palms, rigid boat, and two birds. Existing faces
and variations are retained. Use Save variation before experimenting, or Undo to
restore an edit. Move, resize, recolor, duplicate, hide, remove, and reorder these
layers; Deploy USB > Upload selected face saves the edited layout to the board.

The default face is black, with full-width sea and the scene shifted down eight
pixels. The three waves retain their offset timing, and the boat uses whole-pixel
translation with four pixels of travel and remains in front of the waves. Clock
and date remain live on the board. Native elements keep speed 1 and no extra
movement on upload; convert to a sprite to draw new pixel frames. Island's native
clock is 12-hour and its date is MMM D; the standard clock/date widgets retain
their separate supported formats. Bird flight pauses are repeatable in the
browser preview and random 20–30 seconds on the board.

`island-assets.js` mirrors the native pixel geometry. FLS2 kinds 5–16 identify the
twelve native elements; their position, size, and two editable colors are stored
in the existing layer headers. Updated firmware is required once to accept these
kinds; uploads remain 200 bytes for the default twelve-layer face. The board uses
its canonical Island drawing routines for both built-in and uploaded elements.
Scratch frames use external RAM to preserve the internal memory needed by Wi-Fi.

With the original Island board-layout face uploaded, run the opt-in hardware check:

```powershell
.\.tools\host-python\Scripts\python.exe scripts/verify_island_upload.py
```

It checks forty native/uploaded frame pairs, boat rigidity and travel, full-width
sea, reboot persistence, Wi-Fi reconnection, and internet time synchronization.
The portable import is `artifacts/island-editable-library.json`.


## Island weather scene

The named Island — Adrian face is selected as the default on first weather setup;
its existing geometry is preserved and five weather layers are appended. Use the
Scene weather panel to choose a city (Boston, MA initially), retrieve current
conditions, or preview sunny, partly cloudy, cloudy, rain, thunderstorms, or snow.
The Open-Meteo current API refreshes every ten minutes while Studio is open;
read failures retain the last successful result and display an error. The panel
shows the resolved city, observation time, temperature in Fahrenheit, and whether
a preview override is active. A city/state or city/country search changes location.

Sun, clouds, rain, snow, and lightning remain separate editable library widgets.
Their placement, dimensions, visibility and colors use the normal layer controls.
The weather panel controls rays, cloud count/movement, rain density/speed/length/
slant, snow density/speed/fluff/drift/sway, and lightning minimum/maximum gaps,
duration and width. Lightning defaults to a thin 0.12-second strike every 60–120
seconds. Preview lightning strike pauses on a strike without changing the schedule.
Rain/snow/lightning are layered over the island scene. Settings persist per face
and survive workspace export/import and Save variation.

The weather-enabled board firmware supports these effects as procedural layers,
not short baked loops. FLS2 kinds 17–21 carry a validated 76-byte payload with
condition mode, enable/rays flags, fifteen visual settings, widget speed and
resolved latitude/longitude. Uploads require weather_version=1 in board status.
Boston coordinates are built in; refresh other locations once to resolve them
before compiling. Positions, dimensions and both colors remain in layer headers.
Native Island layers also support flips and right-angle rotations using header
flags, preserving edited wave variations.

With Current weather selected at upload, the board fetches weather_code directly
from Open-Meteo over certificate-verified HTTPS every ten minutes. It does not
need Studio to remain open. The worker retains its last good result on failure
and retries after thirty seconds. Location/visual settings persist with the scene;
test overrides are temporary and do not replace the saved layout. Clock/NTP and
Wi-Fi configuration are retained. Conditions remain unknown (no weather overlay)
until the first successful fetch, rather than showing invented conditions.

USB diagnostics: weather status, weather test sunny|partly-cloudy|cloudy|rainy|storm|snowy,
weather strike, and weather auto. The last command restores the configured live
condition. The opt-in scripts/verify_weather_board.py cycles all six conditions,
captures PNG/GIF animations, verifies moving precipitation and lightning, waits
for a naturally scheduled strike, then restores automatic weather. It opens the
board only when explicitly run. Captures/reports go to artifacts/weather-board/.

Sources: https://open-meteo.com/en/docs and
https://open-meteo.com/en/docs/geocoding-api. Current route:
GET /api/weather/current?location=Boston%2C%20MA (loopback server only).


### Board deployment verification

The native weather renderer is tested with the saved Adrian layout, including
its clock/date positions, edited sprite boat and flipped wave variation. Board
captures use optional `capture rle` encoding so short lightning flashes and motion
can be inspected without flooding USB. Normal `capture` remains compatible.
The hardware test saves six 24-frame GIFs, condition PNGs, a captured lightning
strike, a contact sheet and verification.json. Natural lightning is checked
separately from the explicit test flash. The final weather status must itself
report a successful weather sync; unrelated NTP heartbeat output is not accepted.

HTTPS uses the verified certificate bundle, IPv4 and a separate task on core 1.
TLS allocations, the weather task stack, the LVGL transfer buffer and capture
scratch memory use external RAM, leaving internal RAM for Wi-Fi packet buffers
and the HUB75 DMA driver. Display geometry and timing remain unchanged. Failed
requests keep the last successful weather condition and retry; temporary test
conditions are cleared with `weather auto` and never replace the saved scene.


## Shared workspace and recovery

Designs live in data/workspace.json, outside browser storage and ignored by Git.
The server writes an atomic replacement only after validating collections and
checking the caller's revision. Earlier workspaces and migrated browser copies
are retained as content-addressed JSON in data/recovery/. Back up the data folder
to move the complete shared library to another computer. This is shared local
storage, not public cloud hosting or LAN access.

Existing browser storage remains a recovery cache. On first setup, its workspace
is migrated if the server has no saved workspace. When a shared workspace already
exists, differing browser data is archived to data/recovery/ rather than replacing
the shared library with an older or blank browser copy. Existing backup JSON can
be brought back with Import. Open browsers check for changes every three seconds,
pausing updates during active edits or dialogs. Stale saves receive a conflict;
the unsaved local copy is retained and the UI asks for a reload, preventing silent
overwrite. Refresh already-open dashboards once after installing this update.


## Adrian2 minute birds

Island — Adrian2 uses five independent minuteBird layers (birdIndex 0–4), replacing
only that face's two legacy bird layers. Other faces keep their original birds.
The five original three-pixel birds perch at (9,43), (17,42), (20,47), (25,46),
and (30,49). They enter alternately from left/right starting at clock seconds
10,20,30,40,50, glide for two seconds, and stay perched through 59.999. All take
off together at second zero, clearing the screen by second four. The remainder
of seconds 0–9 is empty. Timing follows wall-clock seconds, including fractions,
not the scene animation playhead. In Studio select Computer time or enable Tick
clock for a sample-time preview. Position, size, color and timing identity survive
saving and library reuse. Keep speed 1 and extra transforms off for upload.

FLS2 kinds 22–26 are payload-free minute birds, requiring minute_birds=1 in board
status. Their 64x32 native flight envelope begins at y32. The firmware keeps old
Island rendering compatible and uses the same glide/flap equations as the preview.
The diagnostic birdphase <0..60>|auto changes only the birds' render phase for
hardware checks; it never changes the clock/NTP time. The opt-in hardware script
scripts/verify_minute_birds.py checks the count milestones and rollover, saves
board captures in artifacts/adrian2-birds, and restores birdphase auto and weather
auto in a finally block. Shared workspace recovery preserves the original face.


### Bird motion timestamp consistency

Every display frame captures wall time with one gettimeofday call. The displayed
clock and all five minute birds use seconds and microseconds from that snapshot;
scene_player_render receives the fraction explicitly and never reads a fresh
fraction for individual birds. Previously a whole-second rollover between separate
reads could fabricate a one-second rewind (e.g. 11.999 -> 11.001 -> 12.032), moving
a landing bird backward for one frame. This is distinct from panel refresh ghosting.
Regression coverage exercises all arrival/departure paths at one-millisecond
increments around second boundaries and reproduces the old split-read jump.
