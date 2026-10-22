# Player step

Target: `Df.exe` `0x482B00`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`step_player` runs the required weapon prefix first. It then updates recoil,
eye height, and waypoint selection. It reads the current local actor after the
prefix. The waypoint distance keeps the original signed comparisons,
unsigned radius tests, and 32-bit arithmetic.

Bind `PlayerStepWorld.weapon_prefix` to the complete `0x482B11..0x482B78`
weapon section and its reached effects. Bind `actor` to current records.
A zero local actor skips the whole function. A missing reached binding or
waypoint entry raises `PlayerStepError`.

Run the local original comparison:

```sh
.local/tools/python/bin/python tools/check_player_step_original.py \
  .local/reference-game .local/root-player-step/player_step_probe
```

The checker executes original `0x482B00`. A declared synchronous state-change
fixture replaces the weapon prefix at `0x482B11` and resumes at `0x482B79`.
The comparison covers the remaining original instructions, including recoil
limits, eye arithmetic, waypoint limits, prefix changes, and the null actor gate.
