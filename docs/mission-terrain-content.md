# Terrain content values

Use `mission_terrain_content` after successful `load_terrain_runtime` with the same AssetStore. Publish its nine words to the current BD67D4..BD67F4 owner shared with Ready.

Read the original decoded assets through AssetStore. Values cover color, elevation, character, detail color, detail elevation, detail shade, water, sky and 54 sky-gradient palette bytes. CRC uses polynomial 04C11DB7, initial FFFFFFFF and no final XOR. Runtime sky presence and final water_height select the optional source gates. Required missing images throw.

`mission_terrain_crc` ports 456F30 with current byte and 4F8720 table reads. Supply bounded memory. Its do-while loop reads once even when count is zero.

This helper supplies the nine outputs of successful 47B590 startup. TerrainRuntime retains resource and render state. Original 47B590 allocation, progress, error callbacks and palette setup have separate service boundaries.

Source: Df.exe SHA256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff; 47B590..47BE8F, 456F30..456F62, PCX loader 475280..4756C2.

Run `native_mission_terrain_content GAME_FOLDER` and `check_mission_terrain_content_original.py GAME_FOLDER PROBE --report REPORT`.
