# DF system receiver

Use `mission_df_system_receive(bindings, frame)` for source `4B9400..4B9511`.

Map 0x9C current frame bytes. Keep the local name record at 0..87 and the packet pointer at 98. Preserve incoming local bytes. Bind current reads, stores, required calls, and the same DF/DLL memory used by the transport.

The body increments DWORD `501358`, reads the packet WORD opcode, and reads the current target from `4B93F0`. It handles these source blocks:

| Opcode | Block | Current effect |
|---|---|---|
| 1050 | 4B9432 | Set peer501344; increment501348/50134C; call GetPlayerName; publish frame80/84 to50136C/501370; call501374 |
| 1051 | 4B94A2 | With501368 set, publish peer, decrement501348, and call501378 |
| 1052 | 4B94D9 | Call501380 |
| 1053 | 4B94F2 | Read current packet peer and pass it to501384 |

Each callback uses its source gate read and later current target read. Bind the reached target, including a changed target. Unknown jump targets raise an error. Counter arithmetic wraps at32bits.

Bind `4C5520(peer, output)` to the existing DLL GetPlayerName body with the same mapped output address. The receiver ignores its returned status. It reads current frame80/84 after that call, including when the name service writes no output or only part of the record.

Keep one owner for DF cells, the current D0A540 allocation, DLL state, player records, and heap buffers. Keep each frame and reached allocation mapped through callbacks. Pin allocation lifetime and permit source callbacks to change current bytes. Serialize shared memory access in the host. Release allocator and global memory locks before callbacks, blocking DLL calls, or thread joins. A callback can call the transport again; keep that source call order and current buffer reads.

The native test joins the real queue, GetMessage, name producer, DF transport receive, this body, and explicit callbacks. It checks the recursive next packet, failed name output, late callback replacement, and missing-service write prefixes. It uses declared active-host/player/queue inputs and sends no outside packets.

Source: supplied `Df.exe`, SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. REA evidence: `ev_d5eeb939a6aa36709f4dfa43b6031e2eb4a2691693dad247ea305e17de9bccf4`, retained in `.local/reports/mission-df-system-receive-4b9400-source.json`.

```
python tools/check_mission_df_system_receive_original.py GAME PROBE --report REPORT.json
```
