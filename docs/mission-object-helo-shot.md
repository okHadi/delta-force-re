# Helicopter shot

`mission_object_helo_shot` owns `450830`. Map the current actor, physical person,
definition, and aligned 28-byte frame through one raw-memory owner. Use a
separate frame for each active call.

Bind `450630` to the current helicopter pose helper. Bind `477620`, `477770`,
and shared `477860` to their real bodies. Keep `45EA00` required and let that
body read the current actor weapon/person. The caller stores weapon 12 before
the pose call and uses its captured person for target-copy reads. The pose
callback can change the weapon/person used by the final shot body.

The caller preserves the `7E2`/`7E5` signed angle gates, two jitter draws,
current frame reads, actor `+B8` bias, and error write prefix.

Run `check_mission_object_helo_shot_original.py GAME PROBE --report REPORT`.
The check compares all parent reads/stores/calls, complete record/frame bytes,
angle boundaries, shared random state, callbacks, and failures. Real-angle mode
executes the original angle/square-root instructions. Pose and final shot calls
use explicit fixtures.

The native test joins the real pose caller and existing shot/spawn helpers with
one canonical group-three actor/person owner and projectile ring. The existing
spawn adapter supports group three. Other actor groups require a separate
source-backed owner and wire-format binding.
