# Input bridge

Create one `InputBridge` with the current profile bindings. Feed each window
event in poll order. Keys use Windows VK values. Buttons use 1 left, 2 right,
3 middle; legacy button 0 means left. Supply relative pointer movement through
`pointer_delta`.

Call `frame(mission_tick,flags)` once per mission update. Use the mission tick
counter for queue timestamps. Supply current input mode, live-input gate,
window state, mouse scale/flip, steady gate, joystick buttons and global-key
state. Use `set_live_sources()` to read canonical game cells before each key
and sample stage and to execute global effects and chat bodies synchronously.
The synchronous global callback owns direct dispatch and effects. Returned
`immediate` and `effects` lists contain observations. Apply each body once.

Key presses update VK state and the 128-slot ring. Repeated presses use the
same producer. Releases clear VK state. `feed_character(byte)` writes the
character byte of the last ring slot. Window `text_input` supplies ASCII
characters; supply a game-encoding conversion for other characters.
Set `keyboard_ring_blocked` from `D096BC`; VK writes remain active.

Focus loss clears VK state and window activity. Pending ring entries, mouse
buttons and unread movement retain their source values. The live/fullscreen/
active mouse gate consumes relative movement when it samples the axes.

Use `query_active(row_index)` for `47A290`. This argument selects a table row.
Supply loaded rows beyond the profile table with `set_query_row_source()`.
The source executable's row `8C` has zero key/mouse/joystick words; command
ID `140` has a separate profile row. Keep these values separate.

Send `frame.commands` to the mission command pump in order. The frame starts
with `A3`, which clears per-frame control bits. Global immediate calls occur
during input generation, before queued command consumption.

Run `native_input_bridge` and `native_input_state`. Run
`check_input_bridge_original.py GAME INPUT_BRIDGE_PROBE --report REPORT`
for keyboard-ring, focus, mouse gate, profile, query and live-source replay.
Run `check_input_state_original.py GAME INPUT_STATE_PROBE --report REPORT`
for the full input-stage checks.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
