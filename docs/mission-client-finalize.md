# Current client finalization

`mission_client_finalize` runs `440640` against current memory. It scans all 17 client records when `67F528` is nonzero.

Bind `45D520` to the current actor factory and `43F610` to current pose setup. Supply actor and person memory from their current owners. Actor addresses use physical source pointers.

The pass publishes each new actor at client `+08`, copies client peer `+00` into actor `+08`, sets actor team `+70`, and copies the current client name into the current person's `+04` field.

Keep reads current through every callback and write. Keep memory bounds strict, including null actor and person accesses.

The live `45D520` binding needs current definition lookup `4828C0`, character allocation `42CCE0`, and person allocation `44A0C0`. Publish results into the canonical actor and person pools. Resolve raw definition pointers at actor `+04` through the current definition table.
