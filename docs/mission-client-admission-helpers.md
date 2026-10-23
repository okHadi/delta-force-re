# Client admission helpers

Bind these helpers to the current source memory used by `mission_client_added`.

- `mission_client_clear_references` runs `442450`. It clears matching client references in each active client's 17-byte list at `+1D0`.
- `mission_client_name_message` runs `43F770`. It substitutes `$A` with the current name and returns shared output address `78EE54`. Supply complete memory backing for every reached read and write.
- `mission_client_peer_name` runs `497870`. It reads current table pointer `501398`, then calls its entry at `+6C` with the peer ID. Bind that target to its transport owner.

Keep memory bounds strict. Keep source strings and shared output current through each callback. Keep the client pool in `MissionClientLive`.
