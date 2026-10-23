# DeskDisplay Face Lab

A local, dependency-free sketchbook for selecting 64×64 clock faces before
implementing them on the ESP32. Nothing connects to the board or a remote service.

## Open

Double-click `preview/index.html`, or run from the repository root:

```powershell
.\scripts\preview.ps1
```

Then open <http://127.0.0.1:8765>. The server only exposes the `preview` directory
on loopback. Use `-Port 8766` if needed. Keep the same address/port to retain the
same browser storage. No package installation or firmware build is required.

## Design workflow

1. Compare the six synchronized faces. Island and Ribbon adapt the current
   firmware; Orbit, After rain, Alpine, and Firefly garden explore alternatives.
2. Click a preview to edit its palette, digits, layout, and motion. Try both
   crisp pixels and LED dots. The brightness slider approximates screen appearance;
   it does not model panel power or the board's brightness percentage.
3. Pause, step by 1/30 second, or scrub the first two minutes. Playback can
   continue beyond two minutes; Restart returns to zero. Speed controls animation
   and the optional ticking sample clock. Computer time always follows the computer.
   Scene speed scales scene time (zero freezes the scene).
4. Use a sample time to check 09:59 → 10:00, midnight, or long dates. Enable
   **Tick clock** to advance it with the animation timeline. Computer time uses
   the browser's timezone, which may differ from the board's configured timezone.
5. Star favorites, add notes, and **Save as variation** to keep alternate settings.
   Edits save automatically in browser storage. Reset restores the selected base
   face's visual settings while retaining its name, notes, and shortlist status.
6. **Export workspace** backs up all settings, notes, and variations as JSON.
   Import merges faces and keeps existing work; changed originals become variations.
   Import restores the exported clock/view settings. Maximum: 60 faces.
7. **Save PNG** exports a native 64×64 RGB565-quantized frame, without the simulated
   dots or screen brightness. **Copy build brief** gives the selected settings and
   notes to paste into a request here. If clipboard access is blocked, it downloads
   a text file instead. Include the workspace JSON when sharing a complete design.

Storage is specific to the browser and URL, and clearing browser data removes it.
Export before moving to another browser, machine, or address. Browser security
settings can prevent storage when opening a file directly; the local server is the
preferred way to work. No design is automatically synced into firmware.

## Fidelity and adding concepts

Every face is drawn into a 64×64 canvas, with integer pixel placement and RGB565
quantization. `engine.js` is a pure renderer, `app.js` manages the gallery,
`workspace.js` validates and merges saved work, and `styles.css` controls presentation. No fonts or scripts load
from a CDN. Add a preset and a rendering branch in `engine.js` to explore a new
scene without changing the gallery infrastructure.

The JavaScript scenes are **concept previews, not a board emulator**. Ribbon's
header/date use the compact preview font rather than LVGL's UNSCII font. Island's
bird schedule is deterministic (departure at 24 s, return at 48 s in a 52 s cycle)
rather than randomized. The segment font, colors, and scenes follow the existing
firmware where practical, but this renderer does not execute its C sources.

After selecting a concept, implement it in `main/ui/`, optionally introduce a
desktop LVGL target sharing the C renderer, then verify physical brightness,
readability, DMA behavior, memory, and sustained frame rate on the board.

## Checks

```powershell
node --test preview/engine.test.cjs preview/workspace.test.cjs
node --check preview/app.js
```

The renderer tests check deterministic seeking, valid opaque frames across all
presets and layouts, color quantization, and frozen motion. Workspace tests check
portable settings, repeated imports, validation, and atomic merging at capacity.
Browser QA covers selection, controls, local saving, exports, and shortlist.
