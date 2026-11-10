# Board face menu

Open the board's Wi-Fi dashboard. Its **BOOT button menu** lists the faces saved
for playback, with a **Showing now** indicator. Tap BOOT to advance through them.

- **Add ... to button menu** publishes the face currently open in the editor.
- **Update ... on board** replaces that face's saved playback version.
- **Show** displays a saved entry; **Edit** opens its editable library design.
- Drag cards to change the button order, or use the arrow buttons with a keyboard.
- **Remove** takes an entry out of playback and retains its editable library design.
- The built-in selector can add Island, Ribbon clock, Cellular automata, or Diagnostics.

Menu changes save immediately. The first entry opens after a restart. At least
one entry must remain. Editor previews run live, and editable library changes
save automatically, but publishing the playback version requires Add or Update.
Pressing BOOT during a preview returns to the first saved menu entry.

The menu supports up to 32 entries within 512 KB, including compiled animations
and names. Large animated faces may fill it sooner. Two CRC-protected records in
the existing scene partition retain the previous menu if a write is interrupted.
Upgrades retain the previous saved custom scene and add the existing game and
Diagnostics entries. USB scene deployment is disabled once this menu exists;
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
