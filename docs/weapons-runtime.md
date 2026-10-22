# Weapon runtime

Target: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`WeaponState` owns timers, ammunition, slot selection, and the aim blocker.
`player_weapon_step` joins the complete `0x482B00` weapon prefix to recoil,
eye height, and waypoint updates. A zero local actor skips the function.

## Live bindings

Set `WeaponShotMode::live_body` and bind current state through
`PlayerWeaponBindings`. The scoped prefix restores prior callbacks after
return or error.

- `burst_storage` resolves current local person `+0x6C` on each access.
- `input_query` runs `0x47A290(0x8C)` only at the gun expiry gate.
- `effect` completes sound, queue, and UI effects at their source call points.
- `view_body` completes the reached gun, launcher, melee, claymore, satchel,
  or grenade view block before later pending and timer writes.

`execute_player_view` owns the view branch, recoil, placement offset, and
weapon 15 position change. Bind the current actor source, steady query,
animated action `0x45E7A0`, muzzle path `0x482D90` / `0x4847D0`, full shot,
placement, actor position write, and separate CRT RNG `0x49AF6C` as reached.
The shot callback receives its exact original call site and actor handle.
Read that actor's current `+0x94` weapon at the call point.

`execute_player_shot` implements `0x45EA00` for all 39 weapon IDs. It runs
spread, melee, and projectile calls in source order. Bind its burst source to
the shot actor's current person pointer. First shots set the burst before
spawn; follow-ups decrement the current burst cell after spawn. The view
provider executes this body once. Live `WeaponState` does not repeat its
burst writes.

Supply the shared `0x477860` RNG source for shot spread and aim sway.
`toggle_player_aim` implements `0x484C20`: enabling sets the blocker before
RNG and sine/cosine reads; disabling clears it and preserves sway.

## Isolated fixtures

The default `isolated_fixture` mode keeps its internal burst cell and the
weapon 6 shot fixture. `tick(bool)` supplies a fixture held value when no
lazy query is bound. Use these inputs for component tests. `on_event` runs
before later writes; `take_events` clears recorded observations.

## Tables and checks

The static tables contain 39 weapons at `0x4F9934`, 28 classes at `0x4F9154`,
and 28 loadout recipes at `0x4F9704`. The static weapon `+0x98` words are zero.
`0x46BFB0` fills runtime definition indices from primary and secondary item
IDs; resolve these indices against the mission definitions for rendering.

```sh
python3 tools/check_weapons_original.py GAME_FOLDER WEAPONS_PROBE
python3 tools/check_player_weapon_runtime_original.py GAME_FOLDER PLAYER_WEAPON_RUNTIME_PROBE
```

The component replay checks timing, ammo, tables, and ordered effect fixtures.
The live replay executes original `0x484C20`, `0x45EA00`, firing paths, and
complete `0x482B00`, with real shared RNG and sine instructions. It compares
raw state and call order with declared geometry, input, CRT, and effect
fixtures, including effects that change person pointers and player state.
