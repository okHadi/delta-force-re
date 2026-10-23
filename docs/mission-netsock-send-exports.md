# Netsock send exports

Use one current packet memory, allocator and queue owner with these cdecl entries:

| API | Source | Frame size | Arguments |
| --- | --- | --- | --- |
| `mission_netsock_send_to_client` | `10001CF0` | `10` | data/length/peer at `4/8/C` |
| `mission_netsock_send_to_server` | `10001DC0` | `C` | data/length at `4/8` |
| `mission_netsock_send_player_lookup` | `100019E0` | `8` | peer at `4` |

Sizes and offsets are hex. Keep each frame mapped through return and release all nested frames.

Read current ready, TCP and active gates. Client sends require role `1`; server sends require role `2`. Cache caller data and full DWORD length at their source reads. Lookup uses wrapped `peer-10000`, unsigned current count comparison, current player base and current player `+9C`.

Cache current scratch `19C60`. Write WORD `length+C` at `+2`, then WORD `1080` at `+0`. Copy the full cached payload forward to `+8`, in DWORDs then bytes. Keep scratch bytes `+4..7` and the four queued tail bytes current. Keep WORD length wrap separate from the full copy count. Let the current memory owner report a fault at its reached access.

For `CF0`, read the cached player's current `+A4` after the copy, then that link's DWORD port `+14` and IP `+4`. Read the cached packet's WORD length last. Pass the full port DWORD to queue `19B20`, priority `0`.

For `DC0`, read current DWORD `19C70`, then current server IP `19B38`. Use the port's low WORD. Read the cached packet's WORD length last. Pass these values to queue `19C38`, priority `0`.

Both exports use the actual `10003B20` queue body. Keep its allocator, free, Sleep and diagnostic services required. A completed enqueue returns export status `0`; source state/data/role/player gates keep their original status values.

The native test declares TCP-ready state, work-buffer settings and socket identifier `55`. It runs actual Init/HostSession, a host-owned `4930` thread, `CF0`, queue, worker drain and packet encoder through one backend. It checks the header, four retained tail bytes, full queued port and captured wire bytes. The current admitted player row is a declared fixture until CreatePlayer/admission is bound. The client test declares incoming role/server cells and runs `DC0`, actual client queue and `3290` drain. Bind raw JoinSession for production client startup. Test OS providers capture output and declare an empty receive source. No outside packets are sent.

Compare the original instructions with:

```
python tools/check_mission_netsock_send_exports_original.py GAME PROBE --report REPORT
```

Use the supported DLL SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. Exact source is retained in `.local/reports/mission-netsock-send-exports-scope.json`. The checker covers the three bodies and actual queue instructions, current mutations, overlap copies, wrapped IDs/lengths, required calls and failure prefixes. Replay its SHA-checked instruction tape with the sanitizer probe.
