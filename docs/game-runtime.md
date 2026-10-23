# Game mission state

Keep the mission asset cache and initial resource tables alive through mission
exit. `Game` owns one `MissionTickState`, animation registry, geometry registry,
muzzle resolver, and profile input bridge for that mission.

Store the selected profile loadout in `MissionTickState::weapons`. Read current
actor images and animation poses when drawing each mission frame. The local
actor supplies the initial eye view. Feed each window event to the same input
bridge. Profile control rows start at `6A0` with a 12-byte stride.

Read `df.cfg` attributes into the shared `67F548` rules word at mission entry.
Use its second byte for health scaling, squad removal, and powerup loading.
Keep the settings defaults when `df.cfg` is absent.

Bind all reached mission frame services before calling `mission_tick`. Run
`begin_frame` at `479750` and pop commands at `479450`. Apply camera, player,
audio, event, effect, and packet services through their current shared owners.

Run `game_flow_probe GAME_FOLDER --start` to check profile creation, campaign
selection, briefing, live scene inputs, and separate saves with supplied data.
This check covers mission entry and rendering. Gameplay checks require live
mission frame services and real control input.
