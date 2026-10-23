# AI shot

`MissionAiShot::fire(ref)` joins `45E7A0`, `45EA00`, and `46FB40`.
Use the current group-three actor reference from the combat effect.
Keep the state, muzzle, animations, loaded items, and projectile owner alive.

The adapter uses the actor's current weapon and person. Spread uses the shared
`477860` random state. The request uses the same 512 projectile records and ring
index as other shots. Bind the reached `470B00` melee body.

The muzzle refreshes its actor image after the graphic callback. Keep the
callback's slot-seven matrix alive through that refresh. Callback changes to
weapon, aim, flags, and person remain visible to later source reads.
The combat caller owns temporary aim restoration. An exception keeps the writes
that occurred before the error.

Run `check_mission_ai_shot_original.py GAME PROBE --report REPORT` to compare
the original muzzle, shot, and request instructions. The comparison checks the
shared random state, person counters, slot matrix, ordered calls, exact packets,
and error prefixes. Graphic, terrain, delivery, melee, and the original allocator
boundary use explicit fixtures. The native test uses the real shared allocator.
