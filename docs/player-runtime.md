# Initial player

Call `initial_player(mission, terrain.geometry)` for single-player slot 0. The result contains the marker index, marker object ID, actor pose, and camera pose. An empty result means the map has no enabled type-6001 start marker.

Positions use signed 32-bit fixed-point values. Angle words use unsigned full turns: one turn is 2^32. The order is yaw, pitch, roll.

Read markers from BMS object group 2 in file order. Skip raw flags `10` or `80` hexadecimal in single player (`449730–44979D`). Select the first remaining type 6001 (`43F610`).

Convert yaw with the original `4497B5–4497EA` sequence: wrap `(90 - degrees) << 16` to 32 bits, divide as signed by 360 toward zero, then shift left 16 with wrap. Pitch uses the same conversion without `90 -`. Roll starts at zero.

Add `TerrainGeometry::base_height(x,y)` to raw marker Z with 32-bit wrap. Original `403D93–403DC4` performs this step before player creation. Copy the marker pose to the actor. Add `B333` hexadecimal to actor Z for the initial class-3 camera (`42A6E0`, default height from `4829F0`).

Run `player_probe GAME_FOLDER --all` for map coverage, or pass one BMS filename. Run `check_player_original.py GAME_FOLDER PLAYER_PROBE --report REPORT.json` for original x86 checks. The checker verifies angle conversion, single-player flags, terrain height, start-marker selection, and camera pose. It checks source SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff` and uses the supplied executable only as read-only test evidence.
