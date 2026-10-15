# Mission state

Call `assemble_mission_runtime` with loaded resources, the item catalog, terrain geometry, optional campaign names, and `MissionInitConfig`.

The load order is: placement slots, item callbacks, BMS groups, names, resource handles, terrain heights, placement actors, live type counts, then local player binding. Model and actor references use stable native ids. Keep group slots stable.

Use `mission_actor_image` to read the complete initialized actor record. `scene_model_records` supplies groups 3, 0, and 1 in draw order.

The game uses the selected profile name and face, the mission start marker, and its loaded local actor for the initial camera.

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `403A60`, `444C50`, `449710`, `402EC0`, `440640`, `44A110`, `4829F0`.

The original replay checks all 62 missions, including 25,492 group records, resource binding, heights, placements, and local player state.
