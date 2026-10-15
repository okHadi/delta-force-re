# Movement rules

Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`mouse_look` applies original `479630` sensitivity, mouse flip, and steady-aim
scaling. It returns signed turn and look values for absolute actions `9B–9E`.
`apply_look_action` applies those values with 32-bit angle wrap.

`apply_look_step` uses control bytes `C23A00` and `C23A01`. The normal step is
`1FFFFFE`; `NoRun` reduces it to `FFFFFF`. Opposite turn or look bits cancel.
Pitch uses original unsigned limits: values through `80000000` are capped at
`40000000`; higher values below `D0000000` become `D0000000`.

| Action | ID | Control byte and bit |
|---|---|---|
| Back / forward | `8E` / `8F` | byte 0, bits 2 / 1 |
| Jump | `90` | byte 1, bit 2 |
| Look down / up | `91` / `92` | byte 0, bits 6 / 5 |
| Slide left / right | `93` / `94` | byte 0, bits 3 / 4 |
| Turn left / right | `95` / `96` | byte 1, bits 0 / 1 |
| NoRun | `9F` | byte 0, bit 0 |

`move_octant` follows original `4881BC`: forward is 0, left is 2, back is 4,
and right is 6. Diagonals use the odd values. `move_anim_selector` returns 0
for idle or conflicting pairs, 2 for actor flag `6E` bit 3 or `NoRun`, and
3 for other movement. These rules use `4884FA–488650`.

`apply_step` follows `487658–487674`: add dx to X, dy to Y, and vertical
velocity plus dz to Z. Keep vertical velocity unchanged in this step. Supply
the original animation and collision result as the step vector.

Build and run `native_movement` for bounds and wrap checks. Run
`tools/check_movement_original.py GAME_FOLDER MOVEMENT_PROBE --report REPORT`
for the original x86 comparison. The checker requires matching output counts,
finished original blocks, and zero mismatches. Its mouse tests preset the
axes and steady-aim state and capture queued actions.
