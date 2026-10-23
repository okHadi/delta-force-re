# Shared client records

`MissionClientLive` owns the 17 current client records at source address `6E9A90`, with stride `9B84`.

Bind `claims`, `read` and `write` before the sound memory fallback. Use `client(slot)` for outbox, parser and round services. Each view uses the same bytes as source memory.

Keep the owner alive while views are in use. Run `mission_client_startup` to set startup fields. Use the current placement producer to set actor fields.

Read and write widths are 1, 2 or 4 bytes. Complete accesses can cross word and record boundaries. Partial pool accesses and wrapped addresses raise an error.
