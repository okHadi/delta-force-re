# DF Join parent

Use `mission_df_join(bindings, frame)` for source `4B9090..4B91B0`. Map0x118 current bytes: local settings0..FF, saved-register/return gap100..10F, selected-session pointer110, password pointer114. Keep source aliases and frame lifetime through required calls.

The body caches both input pointers before reading501320. It calls the real StopEnum service, clears its settings through49A620, sets size100, copies the selected16-byte key toD0, and copies the current16-byte application key at4CEBC4 toE0. Both copies use forward DWORD reads and stores. The raw instruction bytes at4B90F9 and4B911A are `F2 A5`, with count4. Preserve repeated-copy order and overlap.

Copy current password bytes from the cached pointer to settings90 until NUL. A null password writes a zero byte. Set501368=0 before JoinSession. Use this status mapping:

| DLL status | DF result |
|---|---|
| 0 | 0 |
| 9 | 1 |
| F | FFFFFFFE |
| 5 | FFFFFFFD |
| Other | FFFFFFFF |

`mission_df_join_stop_enum` exposes source4B9080. Bind4C5544 to the current StopEnum import at IAT4C68D8. Bind4C553E to the current JoinSession import at IAT4C68C4. Bind49A620 to the canonical-memory memset provider. Keep reached services required. StopEnum's returned status is ignored by the parent.

Use the published DLL Close and Join bodies with the same memory, discovery rows, allocator, queues, endpoint and worker owners. Keep allocations and raw frames live through calls. Release shared-memory and allocator locks before Sleep, callbacks or joins. Let workers produce acknowledgements and join them outside these source bodies.

The native test stops an owned enum worker after real1011 discovery, runs the actual Join worker through1021 success, starts the actual client worker, polls completion through this DF parent, and captures real queued data and close packets. It also checks cancellation and rejection. TCP-ready, endpoint, buffers, clock, incoming packets and OS delegates are declared test inputs. No outside packets are sent.

The oracle protects the saved-register/return gap from data writes. It checks the reached prefix when an oversized password reaches that provider boundary. Bind each runtime frame's writable extent explicitly.

Source: suppliedDf.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Exact PE/Capstone instructions are retained in `.local/reports/mission-df-join-parent-scope.json`; raw prefix and Intel execution proof are in `mission-df-join-copy-prefix-proof.json`. Warm REA reports no function entry at4B9090. Original instruction replay covers the complete body.

```
python tools/check_mission_df_join_original.py GAME PROBE --report REPORT.json
```
