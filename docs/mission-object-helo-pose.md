# Helicopter poses

Use `mission_object_helo_smoke_pose` for `4505D0` and
`mission_object_helo_weapon_pose` for `450630`. Bind current actor `+10`,
graphic `+50`, LOD `+A4`, and hardpoint bytes through one raw-memory owner.
The smoke point uses hardpoint 2. The weapon uses hardpoint 4 as its pivot and
hardpoint 8 as its muzzle.

Provide an aligned `0x40`-byte smoke frame or `0x80`-byte weapon frame.
Keep frames separate for active calls. The current output vector/pose remains
mutable and can share a current actor or hardpoint buffer.

Bind required `45D1E0`, `45D400`, `45CB40`, and `45CDA0` matrix services.
Use the same current memory owner for their inputs and outputs. Hardpoint
pointers are reread before every component. The weapon output carries current
actor `+C4` yaw and `+C8` pitch. An error retains earlier writes.

Run `check_mission_object_helo_pose_original.py GAME PROBE --report REPORT`.
The check compares each parent read/store, matrix call, complete buffer images,
aliases, pointer changes, and failure prefixes. Real-math mode checks results
against the original matrix instructions. Typed matrix helper internal access
order is outside the parent contract. Raw-matrix fixtures check mutable parent
boundaries. The native test uses one canonical actor and one loaded model.

The shared pivot helper uses unsigned 32-bit negation for source `NEG`, including
`INT_MIN`.
