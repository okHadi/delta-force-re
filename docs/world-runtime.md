# World placement

Use `place_single_player_objects()` to prepare initial BMS poses. Each result keeps its source group, source index, object ID, and type ID. Use the source indices to select its `MissionScene` appearance.

Skip single-player flags `0x10` and `0x80`. Convert position and angles with original `4497A3–4497ED`. Add terrain base height to Z with 32-bit wrap. Group 3 adds another 65536. Original height loops are `403C93–403DD4`; group order comes from `449C70`.

Run `world_probe GAME_FOLDER --all` or pass one BMS filename. Run `python tools/check_world_original.py GAME_FOLDER WORLD_PROBE --report REPORT.json` for the original-code check. All 24,333 enabled objects in 62 maps match; 1,159 excluded objects keep the single-player filter. The check uses original SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
