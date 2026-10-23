# Helicopter movement

Bind `451590` with `bind_mission_object_helo_move`. Keep the state and raw-memory
providers alive. Use the current physical actor address and one canonical actor
and person owner.

The module owns `451590`, `44FF30`, `44FFC0`, `450010`, and `450090`. Quantized
trigonometry uses the existing exact `477820` table. Bind source reads and stores
at their complete width. Actor `+04` contains the current definition pointer;
actor `+0C` contains the current physical person pointer. Convert typed indices
at the raw-memory boundary.

Bind reached `47DF40` terrain, `450D50` crash, `450830` weapon, `49A620` clear,
and shared `477860` random calls. The body retains its initial person pointer
and target while later reads use current memory. An error retains prior writes.

Run `check_mission_object_helo_original.py GAME PROBE --report REPORT` to check
full record images, read/store order, callback changes, shared random state,
and failure prefixes against the original instructions. Crash, weapon, terrain,
and clear use explicit fixtures in this comparison.
