# Live mission input

Keep one `InputBridge`, current profile, and `MissionTickState`. Use `mission_input_live_sources` with `set_live_sources`. Supply current memory and `GameWindow::active()`.

Publish loaded `GameConfig.mousescale`, `flipmouse`, and `windowed` into the canonical dispatch globals. The source adapter reads these globals, current local actor flags, and `mission_steady`. It reads input mode, live-input gate, priority key, joystick state, keyboard-ring gate, and typing keys from their source cells.

Use `mission_input_live_global` as the synchronous global-key callback. Its required providers execute direct command dispatch, digit selection, and page calls at the original boundary. Digit selection completes before the canonical digit flag clears. Typing clear and Enter write the canonical dispatch globals. Queued Space remains in the existing input queue. Use `mission_input_live_chat` for the required chat callback. Returned effect lists are observations.

The source mode is a DWORD. Values 0 and 1 select game and chat. Other values select the existing skip category. Full-word Boolean flags keep their nonzero meaning. Typing keys must fit the current 16-bit key interface when typing is active.

Keep loaded profile rows separate from command IDs. Resolve the current key for each action. The shipped movement profile uses arrow keys. Preserve the executable query row `8C` through `mission_session_input_query_row`; it is separate from attack command ID `140`.

Standard movement, mouse look, and stance use the existing dispatcher and canonical control cells. Fire, reload, weapon selection, and cycle use the existing weapon bodies. Bind direct immediate dispatch through a host that applies its effects synchronously. Required menu, chat, text, melee, and placement services must have real owners when reached.

The original checker compares live flag conversion and global-key effects with actual `4790A0` instructions. Native tests cover current cells, high-word flags, focus, mouse movement, profile keys, custom bindings, stance pulses, weapon dispatch targets, callback order, and missing-provider errors. Full mission movement, collision, projectile, and ammunition checks use the separate Session test.
