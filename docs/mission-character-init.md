# Current character setup

Use the same memory and seven-register bindings as `mission_character_allocate`.

- `mission_character_clear_scalars` runs `43A080` in source store order.
- `mission_character_seed_pose` runs `43A0E0`. It reads each current packed angle triplet and uses `character_frame_pose` for its scalar conversion before source pose writes.
- `mission_character_select_action` runs `43A150`. Its return is true for source carry-clear and false for carry-set. Bind the reached `43A690` frame decoder.

Supply complete memory for the current model, action table, frames and pose workspace. Keep one current raw instance owner. The source loops keep signed `DEC/JG` behavior, including zero and negative counters.

ESP, EFLAGS and saved-stack storage are outside the register interface. The action return carries the source success result.
