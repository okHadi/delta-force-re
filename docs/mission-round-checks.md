# Mission round checks

Use `MissionRoundCheckBindings` for current memory reads, writes, and reached calls. Each callback runs before the next source read. Missing bindings raise an error and keep prior writes.

- `mission_round_check_end` runs `444630`: menu and local-death gates, game modes 1 through 5, current client and squad scores, marker checks, and result requests.
- `mission_round_check_clients` runs `448590`: remote-client score penalties and linked-item timeouts.
- `mission_round_publish_status` runs `447930`: read the current byte at `6E5884` and send message `30` through `4423F0`.
- `mission_round_broadcast` runs `4423F0`: enqueue slot 0, then current active clients with type 5 in slots 1 through 16. Keep the mutable payload alive during all enqueue calls.

The canonical factory shares `MissionTickState` globals, local actor, group 2 count, actor fields, and health writes. Bind client records, squads, other global cells, physical definition pointers, and foreign calls through the host. Keep each owner alive. Bind `enqueue_441750` to the shared `MissionClientOutbox::enqueue`.

Bind reached `444CA0`, `446DA0`, `477620`, `447FB0`, `4604C0`, and `4605D0` to their actual bodies. The raw interface keeps signed comparisons, 32-bit wrap, source read order, cached values, and partial writes. Marker coordinates use wrapped absolute differences and signed 64-bit products before the source shift.

Keep statistic source `BCFC24`, statistic output `67DBDC`, status source `6E5884`, and received status `C23B78` as separate cells. The message receiver and frame statistic service share the same `BCFC24` owner.

Evidence: `.local/reports/mission-periodic-{444630,448590,447930,4423f0}-source.json`. Each report identifies the supplied `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_round_checks_original.py GAME PROBE --report REPORT`. It compares actual instructions, ordered reads and writes, required calls, all game modes, changing limits and actor links, mutable enqueue payloads, signed limits, and wrapped marker math. Native checks join the status sender to the actual outbox and check failure prefixes. Tests produce no device audio.
