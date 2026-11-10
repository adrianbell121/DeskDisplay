# Board face menu

Open the board's Wi-Fi dashboard. Its **BOOT button menu** lists the faces saved
for playback, with a **Showing now** indicator. Tap BOOT to advance through them.

- **Add ... to button menu** publishes the face currently open in the editor.
- **Update ... on board** replaces that face's saved playback version.
- **Show** displays a saved entry; **Edit** opens its editable library design.
- Drag cards to change the button order, or use the arrow buttons with a keyboard.
- **Remove** takes an entry out of playback and retains its editable library design.
- The built-in selector can add Island, Ribbon clock, Cellular automata, Diagnostics,
  or Reverse Life clock.

Menu changes save immediately. The first entry opens after a restart. At least
one entry must remain. Editor previews run live, and editable library changes
save automatically, but publishing the playback version requires Add or Update.
Pressing BOOT during a preview returns to the first saved menu entry.

The menu supports up to 32 entries within 512 KB, including compiled animations
and names. Large animated faces may fill it sooner. Two CRC-protected records in
the existing scene partition retain the previous menu if a write is interrupted.
Upgrades retain the previous saved custom scene and add the existing game and
Diagnostics entries. New boards also include Reverse Life clock. USB scene deployment is disabled once this menu exists;
publish from the board dashboard instead. The local editor is unchanged.

The board APIs are GET/POST `/api/faces` and POST `/api/faces/select`.
Menu writes include the last loaded `revision` and an `items` array; stale writes
return 409. Each entry has `id`, `name`, and `mode`; custom entries also include
the compiled FLS2 bytes as a hexadecimal `scene` string. Selection takes `{ "id": "..." }`.
The existing same-origin write checks also apply to these endpoints.

The background save worker writes both menu and workspace records from its
internal-memory stack. The HTTP task and large payloads remain in PSRAM.
Run `python scripts/verify_face_menu.py` for the opt-in connected-board check;
it temporarily changes the menu, checks rendered custom faces, cycling and
restart persistence, and restores the original menu. Close the serial monitor first.

## Reverse Life clock

This separate face holds the current HH:MM AM/PM for `time_hold_seconds` after
each minute changes, clears it, then builds the next minute by playing its
forward Conway Life history backward. It reaches the exact clock mask at the
minute boundary and repeats. Opening the face mid-minute joins that sequence.
The original Cellular automata face keeps its continuously running game.

Both faces use `cellular-automata-config.json` for update speed, hold duration,
colors, and gradient direction. Reverse Life clock calculates its history length
from the whole remaining minute: `ceil((60000 - time_hold_seconds * 1000) / update_ms)`.
At 125 ms with a five-second hold this is 440 generations; at 500 ms it is 110.
When the interval does not divide the available time, the first frame is shorter
so the finished clock still lands on time. Frame storage uses checkpoints to
stay below 512 KB, including the cache, even at the fastest configuration.

The original face's `time_reverse_frames`, fading, and scheduled game clears
do not apply to this face. It has no background game or random seeds: each cycle
contains only the next clock's exact Life snapshots. Still or empty stretches
in that history are retained. USB `mode reverse` opens it directly.
Run `python -u scripts/verify_reverse_clock.py` for the connected-board minute,
hold, clear, gradient, four-face cycle, and restart checks.

## Switch engines in the original game

Set `switch_engine_spawn_seconds` in `cellular-automata-config.json` to an integer
from 0 to 86400. The current value is 15 seconds; 0 disables spawning. Rebuild
and flash to apply changes to the board. The first spawn happens one interval
after opening the original Cellular automata face. Its timer continues through
clock overlays and minute clears.

Each spawn adds Corderman's eight-cell seed from the
[switch-engine pattern file](https://conwaylife.com/patterns/switchengine.cells),
using a random position and one of eight rotations/reflections. Existing live
cells retain their ages, and the surrounding game continues normally. Collisions,
the wrapping 64×64 grid, and the configured fading can alter the seed's evolution.
The separate Reverse Life clock never receives these spawns.
