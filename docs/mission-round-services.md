# Mission round services

Use the five source bodies through `MissionRoundServiceBindings`:

| Source | Function | State |
| --- | --- | --- |
| 446100 | `mission_round_set_client_updates` | Write the argument to 6E5588 |
| 444F60 | `mission_round_clear_frame_flag` | Clear 6E5570 |
| 446DA0 | `mission_round_collect_squads` | Clear and collect squad statistics |
| 448130 | `mission_round_compare_clients` | Compare current client scores |
| 448D20 | `mission_round_log` | Append the current round score log |

Bind current cells, 17 client records, and 64 squad records. Client base is
6E9A90 with stride 9B84. Squad base is 6E8090 with stride 68. Offsets and source
addresses use hexadecimal values. Accessors preserve the requested byte, word,
or dword width. Keep reads and writes in source order. Each reached binding is
required; a missing binding raises `MissionRoundServiceError`.

The collector clears squad dwords at 48 through 60. It then reads active clients
with state word 1C4 equal to five, uses their current dword 9C squad index, and
adds client dwords 19C through 1B4 with 32-bit wrap. Keep squad address mapping
consistent with the wrapped source formula `6E8090 + index*68 + offset`.

The comparator reads game mode 67F53C once. Modes zero through two compare
`1A0 - 1B4 - (1B0 << 2)` with wrap. Modes three/four use 194; mode five uses
1BC. Signed comparison orders higher scores, then higher 1AC, then lower 1A8.
Other modes return zero. Equal rows return zero.

The log checks dword 503428, then dword 67F520. Single-player state returns
before file, time, text, client, or squad bindings are needed. Active logging
collects squads, opens `scores.txt` in append mode through 49A41C, and returns
if that call returns zero. Bind `file_print` to 49BAC0 and `file_close` to 49A840.
Bind `local_time` to the eight SYSTEMTIME words from GetLocalTime. File callbacks
complete their state changes before the next current read.

`file_print` receives the source format address and source-order arguments.
Use `mission_round_log_format` for the matching format text. Numeric arguments
carry their original 32-bit bits; `%i` treats them as signed. Text arguments
contain the bytes through the source string end. Supply the current game name
at 67F478 and client names at client offset C. Names up to 31 bytes fit the
source local buffer. Longer names raise an explicit boundary error.

Supply the caller's incoming EDI to `mission_round_log` when team logging reaches
its score reads. Original 448E05 uses that value as a squad index before the
function writes EDI. The blue and red score deductions both use that same index.
The value is an explicit source input. A reached team branch without it raises
an error after the preceding file writes. Stock single-player state returns
before this input is used.

Route the required calls from live mission logic to these functions. Keep
446DA0 inside both result packet building and active score logging. Bind 444F60
at the frame's final source call. Real per-client outboxes remain with their
own source bodies.

## Checks

The source executable SHA-256 is
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Retained 448D20 evidence is in
`.local/reports/mission-logic-builders-root.json`. New REA evidence is in
`.local/reports/mission-round-services-source.json`. Instruction evidence is in
`.local/reports/mission-round-services-disassembly.json`.

Run `tools/check_mission_round_services_original.py GAME PROBE --report REPORT`.
The check executes all five original bodies. It compares complete client/squad
images, cells, ordered field reads/writes and widths, and every formatted-write
argument. File and time bodies use explicit test providers. Cases include
callback changes, signed edges, score ties, inherited indices, log gates and
open failure. The native suite also checks missing bindings, partial effects,
all squad reset fields, source name limits, and required inherited input.
