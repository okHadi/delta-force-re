# Animation driver

`AnimationDriver` runs the animation blocks from object update `487370`:
pending action `4877F4`, motion heading `4873B4`, instance writes and tick `4873D3`,
and low-byte event dispatch `4872C0`.

Use `original_animation_compiled_actions()` with decoded ANM data and
`build_animation_driver_actions()`. The table has 195 named rows from `4EF7C8`.
Each item holds direction, animation number, velocity, override, `weight_10`,
`aim_14`, and tag. C/D become signed 16.16 item fields `+10/+14`.

ANM lines apply in file order. Absent rows have zero fields. Repeated rows replace
the number, replace velocity when supplied, and retain override. Unknown names
raise `std::invalid_argument`.

`tools/check_animation_driver_original.py` executes the original packing chain
`42CF40`, `4565F0`, `42CF60`, `42D159`. It compares all seven words in 256 slots
for full, empty, subset, repeated, and missing-velocity ANM inputs. It checks
1,365 named item fields, 10,752 complete-table fields, 260 event cases, and
222 update operations (7,536 fields). All match. Unknown-name input faults the
original; the native guard rejects it.

Run the checker with `--probe`, `--chr`, `--anm`, `--tags`, and `--report` paths.
The probe uses `-` as the tags path to select the built-in original table.
`tests/native_animation_driver.cpp` checks mapping, pending actions, tick gating,
reserved slots, and event masking. Tests produce no game audio.

Bind `on_action_tag` to run `460D20` before the tick. Read current actor and
instance fields into its `AnimationDriverState` argument after the effect.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Event targets keep address-based names until their effect records are verified.
