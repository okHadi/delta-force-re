# Player weapon runtime

Target: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Use `WeaponShotMode::live_body` for live control calls. Bind
`WeaponState.burst_storage` to the current local person's `+0x6C` word and
`view_body` to `execute_player_view`. The isolated mode supplies component
fixtures.

`player_weapon_step` runs the weapon prefix and `PlayerStep` tail in source
order. Its scoped bindings restore prior burst, input, effect, and view
callbacks after return or error. A zero local actor skips all stages.

`execute_player_view` owns the normal/animated branch, recoil, placement
position, and weapon 15 movement. Supply the reached steady, muzzle,
animated action, actor, position write, placement, and full shot callbacks.
Its CRT `0x49AF6C` RNG source is separate from shared `0x477860`.

The full shot callback receives the original call site and actor handle.
Read that actor's current weapon `+0x94` and call `execute_player_shot` once.
Bind this shot's burst resolver to the passed actor's current person pointer,
including after a spawn changes that pointer. `execute_player_shot` owns
spread and burst changes around each spawn or melee call.

`toggle_player_aim` implements full `0x484C20` blocker and sway state with
shared `0x477860` RNG. Supply current raw sway words in `PlayerAimState`.
The lazy input callback queries `0x47A290(0x8C)` only when gun expiry reaches
its refire gate. Missing reached handlers raise an error.

```sh
python3 tools/check_player_weapon_runtime_original.py GAME_FOLDER PLAYER_WEAPON_RUNTIME_PROBE
```

The replay compares 4,635 cases against original aim, shot, firing, and full
player-tick instructions. Shared RNG and sine instructions execute directly.
Geometry, CRT RNG, input, and effect bodies use declared fixtures with state
changes. The native test checks shared burst writes, prefix/tail composition,
required handlers, and callback restoration.
