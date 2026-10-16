# Weapon runtime

Local player weapon state for Delta Force (1998). Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Addresses are original virtual addresses.

The module covers the deterministic timing and ammunition machine: fire
cadence (483C70 -> 483B10), the action/reload timers (482B00, 483B70,
483C30), magazines and spares (483240, 4832F0, 483390), slot selection
(483DD0, 484B70) and the aim blocker (484C20). Ballistic calls (45EA00
projectile entry, 46FB40 bullet entity) and recoil/camera state stay outside
and are reported as events.

## Tables

`weapon_definitions()` copies the 39 x 0xA8 records at `4F9934`: item ids
`+0x64/+0x68`, action class `+0x6C` (1..11), behaviour class `+0x74`,
muzzle/aim offsets, `fire_delay +0x9C`, flags `+0xA4` (bit `20` = refire
while held). `weapon_classes()` copies the 28 x 0x34 records at `4F9154`:
`+0x04` spare magazines, `+0x08` magazine size, sounds at `+0x24/+0x28/+0x2C`.

`46BFB0` fills the runtime names from RTXT group `Player Weapons`
(`STR_PWNAME%02i`, `STR_SPWNAME%02i`) and maps `+0x64/+0x68` through
`4828C0` to item definitions; the static tables here keep the raw fields.

## State machine

Per behaviour class (`+0x74`): `magazine` (C238B8), `spare` (C23938) and
`pending` (C23838). Shared: `fire_timer` (C23830), `reload_timer` (C23834),
slot (C239B8) and the slot map (C239C0, 10 entries).

- `resupply` (4848C0): spare = magazines - 1, magazine = size, pending = 1.
  With `C23A4C == 1` slots 3..5 get spare = magazines * 2 - 1.
- `fire` (483C70): blocked by `C23B64`, empty slot, and a running fire timer.
  Then dispatches by action class (table `483AD8`).
- `tick` (482B00 weapon part): fire timer expiry dispatches table `483B44`;
  reload timer expiry dispatches table `483C1C`; the action class counters
  `C23A3C` decay; the item definition `+0x6C` burst counter re-enters `fire`
  when the timer has expired, which produces the M4 three-round burst
  (weapon 6, `45EB17`).
- `reload_press` (483BD0, table `483BA4`): guns start the 120-tick reload
  (magazine 0, sound `+0x28`); launchers and thrown weapons play `+0x24`
  and set the fire timer.
- Fire paths per action class: gun 482F90, launcher 483410, melee 4838C0,
  claymore 4840B0, detonator 470F40, satchel 4841F0, laser 483A30,
  grenade 483670. Cooldown is always `fire_delay +0x9C`.
- Expiry per action class: gun 483240 (consume round, refire while held when
  `+0xA4 & 20` and control `8C` is down), launcher 483610, charges 483860,
  melee 4839F0, laser 483AA0.
- Slot select (483DD0) resets both timers, starts a reload for an empty gun,
  re-arms launcher/charge classes and clears the burst counter. Cycle
  (484B70) skips empty slots and wraps; the wrap scan includes the current
  slot, so cycling with one weapon reselects it.
- `set_unlimited_ammo` reproduces the `67DBC9` bit 0 toggle (`484960`,
  message `STRMISC26`): rounds are not consumed.

## Events

Each operation reports ordered events: `shot` (45EA00 burst entry, weapon
index), `sound` (43E7D0), `sound2` (43E720), `place` (470F60 item id),
`queue` (46F3E0 message type) and `message` (479F00 HUD text). Ballistic
callers consume `shot`/`place`/`queue`.

## Checks

Build the probe and test with `/usr/bin/clang++ -std=c++17 -Wall -Wextra
-Iinclude` on `tools/weapons_probe.cpp src/weapons.cpp` and
`tests/native_weapons.cpp src/weapons.cpp`; sign local binaries with
`codesign --force --sign -` before running.

`.local/tools/python/bin/python tools/check_weapons_original.py
"/path/to/Delta Force" weapons_probe [--report .local/reports/weapons-original-check.json]`
verifies both tables against the executable bytes and replays 25 scripted
scenarios on the original x86 code with unicorn, comparing slot, weapon,
timers, per-class magazine/spare/pending, aim flag, burst counter and
event order after every step (4521 lines, all compared equal).

## Open points

Action id to control mapping (`477D40`), the loadout table behind `AD3ED4`
(`484970`), and the recoil/sway values owned by the camera side stay
documented in `.local/opencode/weapons-needs.md`.

## Mission loadout

Call `WeaponState::loadout` with sidearm, primary, and secondary recipe ids. It builds the ten slots from the 28-row table at `4F9704` and applies `4848C0` resupply. Secondary id 1 doubles primary reserve. Select slot 3 after mission initialization, as `4829F0` does.

```sh
python3 tools/check_weapon_loadout_original.py Df.exe WEAPONS_PROBE --report .local/loadout.json
```

The replay checks all 21,952 recipe-id triples, the ten slot values, and all 84 ammo-state words after each loadout change.
