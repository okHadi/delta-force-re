# Mission peer budget

Use `mission_peer_budget_update` for source body 444E30. Bind current shared
storage through `MissionPeerBudgetBindings::read` and `write`. Widths are one,
two, or four bytes. Each reached provider is required.

Client records start at 6E9A90, have stride 9B84, and contain 17 slots. The body
counts clients whose active byte 18C is nonzero, state word 1C4 is five, and
current actor byte 6C has mask 02 clear. Client dword 8 supplies the current actor
address. Bind that address to its actual actor storage. An active state-five
client requires a valid actor. Source reads through a null actor fault; the host
reports an invalid address instead of supplying a default actor byte.

Keep these six dwords with their current owner:

| Address | Use |
| --- | --- |
| 6E5570 | Initial local request, captured before the client scan |
| 6E5574 | Available budget, read after 6E5584 is written |
| 6E5578 | Local result and current final clamp input |
| 6E557C | Other-client result and current final clamp input |
| 6E5580 | Eligible client count, written once after the scan |
| 6E5584 | Captured request, written after the count; current final clamp input |

The body keeps its scan count local. It then uses `count - 1`, local cost 15,
and other-client cost 14 with 32-bit wrap. The budget comparison and divisions
are unsigned. The local-request branch and final limits use signed comparisons.
Preserve the source underflow when no client is eligible. Final limits reread
6E5578, 6E5584, 6E5580 and 6E557C in that order. Owner changes are visible at those
reads. Completed effects remain when a provider raises an error.

Route the live mission logic frame's required 444E30 call to this helper. The
separate 444F60 body clears 6E5570 later in that frame. This module needs no file,
network, audio, clock, or external service body.

## Checks

Retained REA source is `.local/reports/mission-peer-frame-source.json`.
The executable SHA-256 is
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
The port follows actual instruction read/write points. Instruction evidence is
in `.local/reports/mission-peer-budget-disassembly.json`.

Run `tools/check_mission_peer_budget_original.py GAME PROBE --report REPORT`.
It compares all shared cells, all client/actor fields, and every ordered memory
access with its width and written value. Cases include integer edges, zero
eligible clients, owner changes during reads/writes, and original actor-read
faults. Native tests also check missing owners and partial effects. These checks
cover the budget body; real frame integration supplies its shared storage.
