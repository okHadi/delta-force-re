# Netsock close and enum

Use `mission_netsock_close_call` with one current DLL memory, shared allocator, socket, and discovery table.

| Entry | Source body | Frame bytes |
|---|---|---|
| 10001250 | CloseTCPIP | 0 |
| 100012A0 | Close | 0 |
| 100057B0 | Socket and work close | 0 |
| 10005780 | Codec close | 0 |
| 10001990 | StopEnum | 0 |
| 100018D0 | EnumSessions | 0x18 |
| 10003730 | Enum start | 0 |
| 100034A0 | Enum request | 8 |
| 100035F0 | Enum callback scan | 0 |
| 10003680 | Blocking enum worker | 0xC |

`18D0` uses return at 0, config at 4, an unused argument at 8, callback at C, user data at 10, and flags at 14. `34A0` uses return at 0 and IP at 4. The worker uses logical IP at 0, port DWORD at 4, and length at 8. Keep frame bytes live until the call ends. `release` must not throw.

Bind current reads, stores, frame allocation, and required calls. Bind Winsock, Sleep, thread start/end, allocator free, FreeLibrary, and each current enum callback. Use the existing receiver, discovery parser, encoder, JoinCancel, and CloseSession bodies. Read the current DLL callback target for each active discovery row.

Start the owned enum thread on a valid source schedule: the parent must publish its active flag before the worker can finish. The worker sends an initial request, drains packets, sends a new request after 51 polls, sleeps for 20 ms, and scans callbacks before it reads stop. StopEnum writes the stop flag and waits with Sleep(10). The worker clears the active flag before `_endthread`. The host joins the thread outside these source bodies. Keep memory and frames valid through that join. Keep Sleep and join outside memory locks.

Close preserves the source free order. Cache the free import once in `12A0`; read each later pointer after the prior free call. Keep raw pointer fields after free. The host must control final-close lifetime. `1250` returns zero after a returned nested socket error and clears TCP-ready. Required call failures keep their reached write prefix.

The dynamic enum request uses 16 current-port sends. It adds 0x1000 to the current WORD port after each send and restores its cached port after normal completion. Bind current packet memory and the real send body. Preserve the source partial port state when a required call fails.

Source identity: `Netsocknl.dll` SHA256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. Exact PE instructions are retained in `.local/reports/mission-netsock-close-scope.json` and `mission-netsock-close-enum-scope.json`.

Run the original check with the DLL folder and probe path:

```
python tools/check_mission_netsock_close_original.py GAME PROBE --report REPORT.json
```

The native test uses captured datagrams and an owned enum thread. It checks actual `1011` discovery delivery, the final callback after stop during Sleep(20), the worker acknowledgement, external join, close order, dangling source fields, and required-call failures. Its TCP-ready, endpoint, work memory, incoming packet, and OS delegates are declared test inputs. No outside packets are sent. The oracle uses explicit Sleep acknowledgements only as single-thread call fixtures; the native acknowledgement comes from `3680`.
