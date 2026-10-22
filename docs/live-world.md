# Live actor records

`seed_mission_actors` loads converted and factory-created actors into current
tick fields. Keep `images` as the base for fields outside those structures.
Use `merge_live_actor_image` after an update. Keep person slots and person
handles in their supplied encoding.

Pass current images and a current CHR pose source through
`MissionSceneLiveInputs32` on each render call. Keep both alive during rendering.
The renderer reads current slot identities in group order 3, 0, 1.

Run `live_world_probe GAME_FOLDER MISSION` to check every initial actor byte,
compare the initial rendered frame, and check actor removal and pose bindings.
