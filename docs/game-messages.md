# Game messages

Source: `Df.exe`, SHA256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Addresses are original virtual addresses.

## Live use

`GameMessageStage` starts in live mode. Bind `GameMessageHost` and
`GameMessageLiveBindings` before use. Keep the stage, referenced cells and
callbacks valid throughout each call. Keep callback bindings stable while a
callback runs. Missing reached bodies throw an error.

- Bind the shared dedup ring `BCFC3C`, byte counter `BCFC24` and update counter
  `BC5D20` by pointer. Bind global reads and writes to current game state.
- Bind `record_dword` and `record_write` to current actor records. Preserve
  each write width and call order. `call` runs the specified body
  immediately. Payload pointers are valid only during the callback.
- Bind `projectile_09` to `MissionProjectileSpawn::receive(data, length)`.
  This uses the same mutable 15-byte packet during terrain and spawn calls.
- Bind `message` to the complete source-order body for each other reached ID.
  Return true only after that body completes. Missing bodies produce an error.
- Bind `current_update` to current player state and `player_update_0a` to the
  complete player-update producer. Reads after callbacks use current values.

The live router owns IDs `01 09 0A 0B 0D 0E 14 19 1A 1B 1E 20 23 25 29
30 33 34 35 36 37 38 39 3A 3B 3C 3E 3F 41 43 44 46 47 48`. Its external
calls remain required at their source call sites. Other dispatchable IDs require `message`.

`check_receive()` implements `461050`. It uses the peer when `67F528 == 0`
and `67F520 != 0`; otherwise it takes the local echo. It consumes the echo
before dispatch. `dispatch_received()` runs effects synchronously and keeps
`BC5D20` unchanged. `pump_receive()` resets that counter after each dispatch.
Packet bytes stay in the receive buffer for the duration of dispatch.

Actor replication clears the low byte at `6C` before its current dword read.
Case `36` calls the required `sincos_477820` host body and then reads the
current record scale for each velocity product. Keep these record reads and
writes synchronous.

`update()` implements the `46F5C0` order: drain receive, call `482B00`, apply
the tick gate, produce player messages, then flush. The stage owns counters
`BC5D10` and `BCFC28`.

## Queue and frame rules

| Address | Body |
| --- | --- |
| `46F3E0` | Enqueue in the first free slot |
| `46F4B0` | Begin, step queue, flush, compact |
| `4610E0` | Append one message to a frame |
| `4611C0` | Flush the current frame |
| `46D9F0` | Read sequence, queue cancel, check dedup, dispatch |

`flush_frame()` performs only `4611C0`. `flush_queue()` performs `46F4B0`,
including compaction after flush. The client outbox `4421D0` has its own host
body.

The queue has 64 records of `264` hexadecimal bytes, with 602 payload bytes
per record. Frames hold `2000` hexadecimal bytes. Sequenced messages queue a
`14` cancel before dedup; duplicates also queue the cancel. Descriptor reads
after a message body use the current table.

Oversized queue payloads, invalid table patches and truncated receive frames
produce explicit errors or rejection results.

## Inspection

Use `set_inspection_mode(true)` for the separate record decoder and fixture
hosts. `GameMessageAction` values describe inspected effects. Live hosts run
effects through synchronous callbacks. Apply each effect once.

## Checks

`tools/check_game_messages_original.py GAME_FOLDER PROBE --report FILE`
compares real original instructions with the port. The current report is
`.local/reports/game-messages-replication-original-final.json`: 597 cases, zero
differences. These cover table setup, queue, worker, frame, producers,
inspection dispatch, 264 live dispatch cases and four receive flag cases.
Live cases check callback order, callback changes to packet bytes, gates,
dedup, byte counts and queue bytes. Replication cases also compare each
record write in order, its width and the final record bytes. The `09` replay
uses terrain and spawn fixtures; the spawn component has its own original-instruction checks.

`tests/native_game_messages.cpp` also checks current-state reads, missing
bodies, partial effects on errors and echo delivery during a callback. The
native suite passes with AddressSanitizer, UndefinedBehaviorSanitizer and
copy elision disabled. Source, native test and probe compile as strict
Windows objects.
