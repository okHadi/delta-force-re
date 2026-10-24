# Animation controller

`AnimationController` runs CHR1 action selection, frame interpolation, root motion,
heading changes, events, frame conditions, and action loops. `frame_mask()` reads
the current frame flags at `+04`. `set_condition_state()` writes instance `+38`
before the next tick. `pose()` supplies the current bone pose to the renderer. Head overrides use the
frame duration for timed frames and the last Y-angle delta for immediate frames.
A zero head divisor stops after the preceding pose-delta writes. Pitch uses the
source signed byte shift.

Use `decode_animation_definition()` to read decoded ANM text. Lines contain
`name number [velocity] [override]`. Velocity becomes
`trunc(velocity * 65536) / 30`. The parser keeps line order and records whether
velocity was supplied. Use `build_animation_driver_actions()` for compiled items.

`tools/check_animation_original.py` executes the original controller on
PLAYER01.CHR and compares the native probe. It checks action selection, pose,
root motion, frame masks, condition writes, events, heading tables, all condition
opcodes `00`–`40`, and the random selector. The check compares 183 trace operations
(4,192 fields), 390 condition cases, five random cases, and 512-entry heading tables.
All match. Run it with `--probe`, `--chr`, `--anm`,
and `--report` paths. `tests/native_animation.cpp` checks synthetic data and
accepts CHR/ANM paths for real-data checks. Tests produce no game audio.

Source functions: `439E48`, `43A0E0`, `43A150`, `43A690`, `43A2C7`, `43A4DC`,
`43A7A0`, `43AA30`, `43AC10`, `43AC40`, `42CE40`, `42CD90`.
Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
