# Netsock client join

Call `mission_netsock_join_call(bindings,source_entry,frame)` with one current packet memory, socket, allocator, discovery table and queue owner.

| Source entries | Frame layout |
| --- | --- |
| `1420`, `38F0`, `3770` | `8` bytes; argument at `4` |
| `3840`, `37F0`, `3630`, `3540` | `C` bytes; arguments at `4/8` |
| `35A0` | `10` bytes; config/IP/port at `4/8/C` |
| `3960` | logical `C` bytes; IP `0`, port DWORD `4`, length `8` |
| `14C0`, `57F0`, `3A10`, `3570` | frameless; use frame `0` |

Addresses have prefix `1000`. Sizes and offsets are hex. Entry frames include the return cell. Keep incoming worker bytes, including the upper port WORD. `37F0` and `3540` consume WORD ports. Keep the full DWORD at their caller boundaries.

`1420` checks init/TCP state and a caller record of size `100`. A new join clears current host fields, copies the caller record and finds its key in the actual 32-row discovery table. It rebinds the current socket when required, then starts worker `3960`. Pending status is `9`. A later call returns the completed status and clears `199E4`.

Keep the current `199DC` discovery table from Init. `3630` checks a `1011`/`104` reply and its four query-key DWORDs. `35A0/3540/3570` find or fill its real `10E`-byte row. Preserve current candidate reads, cached table base, forward record copies and unaligned fields.

Worker `3960` caches Sleep, polls and uses the existing real receiver. It drains replies through `3840`, then sends `3770` before reading `199E8`. This includes the final request after a success or rejection. Keep its current buffer/capacity, caller arguments, packet fields and string reads in source order.

A `1021`/`C` reply with status WORD `1` sets the server id and reaches `37F0`. This body writes role `2`, active `1` and the reply IP. Fixed mode uses current resolved `198A8` as server endpoint. Dynamic mode uses the reply IP/WORD port. It starts the existing `3470/3300` client queue/worker spine before the reply body writes success and stop state. Keep required failures at their reached call and retain completed stores.

Rejection status WORD `2` maps error WORD `1` to `F`, `3` to `5`, and other errors to `300`. Other reply statuses set the join stop flag and preserve the current result.

Use host-owned join and client threads. `3A10` launches the join worker before it stores active/pending state. Select a valid reply schedule after those parent stores. Keep the client worker from using shared receive scratch while the join worker completes its last request. The native test uses explicit thread gates for this valid schedule. Preserve source stores and keep OS joins outside the port.

Cancellation `14C0` writes cancel/stop, then Sleep/waits for the real join worker to clear `199E4`. A completed result is consumed through `1420`. Keep the worker and memory live through these calls. The production `_endthread` provider ends its thread; a controlled test provider can return.

The native test source-produces a discovery row from a declared `1011` packet, starts a real join thread, injects a declared encoded `1021` through the real receiver, and reaches the real client worker and `DC0` queue/encoder. It checks fixed/dynamic endpoints, pending/success polling, actual cancellation, rejection, close acknowledgement and final requests. Its declared backend inputs are TCP-ready state, resolved address/mode, socket identifier `55`, work buffers, incoming packets and an absent drop file. Output is captured; no outside packets are sent.

Require actual DF parent `4B9090`, enum worker/UI services, host admission `40C0`, OS/socket/codec services and final close ownership before enabling their application paths.

Run the original check:

```
python tools/check_mission_netsock_join_original.py GAME PROBE --report REPORT
```

Use DLL SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. Exact source is retained in `.local/reports/mission-netsock-join-scope.json`. The checker covers all thirteen bodies, current mutations, aliases, fault prefixes, real child queue/start bodies and finite worker/encoder loops. Keep its SHA-checked instruction-output tape for sanitizer replay.
