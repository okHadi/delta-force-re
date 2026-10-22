# Live player

Keep `MissionPlayerLive`, its mission state, player cells and muzzle alive.
Call `install()` before mission command or local-control calls. This binds
live fire, current-person burst storage and view bodies to the owned weapon
state. Use one installed adapter per weapon state. Call `uninstall()` after
mission calls finish; the destructor also restores the prior bindings.
Send stages `482B00` and `484C20` to `stage()`. Chain `nullopt` to the next
stage provider. `weapon_bindings()` supplies bindings for scoped calls.

`MissionTickState` owns the current actor, person, prone state, game type,
scope, weapon state and shared `BD0060` random state. Resolve these values
at each reached read. The full shot follows its supplied actor's current
person, including a person change during projectile creation.

`MissionPlayerCells` owns recoil `C239E8/EC/F0`, eye height `C23A18`, aim sway
`C23B68/6C` and waypoint cells `C23B7C/80/84`. Recoil also supplies muzzle
view movement. Player-block defaults follow `4829F0`; eye height is `B333`.
Set external camera mode `6BA3EC` before a camera-dependent firing path.

Supply real projectile, melee, placement, CRT random, weapon effect and input
bodies. Keep CRT random separate from `behavior_rng_next()`. The adapter
publishes player cells before each foreign call and reads current cells and
mission values after return or error. Weapon effects use
`mission_weapon_effect()` for one trace entry and one provider call.
Providers can change actors, people,
weapon selection and player cells. Keep the adapter on one execution thread.

Normal muzzle calls use `MissionMuzzle::normal()`. Animated calls use its
current slot source and full shot call site `45E946`. Slot providers receive
an actor image for source reads; write mission changes to the mission state.
The adapter keeps actor images separate from canonical actor writes.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `native_mission_player_live` for mutation and error checks. Run
`check_mission_player_live_original.py GAME PROBE --report REPORT` for
original instruction comparisons with declared foreign providers and null
graphic models.
