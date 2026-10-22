# Client outboxes

`MissionClientOutbox` uses the current 17 client records. Each span contains
`9B84` hexadecimal bytes, including its 64 queue records at offset `204`.
Keep those bytes, the shared `MissionServerMessageBuffer`, the message stage
and callbacks valid during each call. Keep client span addresses stable.

- `enqueue(slot,id,data,length)` runs `441750`. It writes the first free queue
  record and preserves untouched bytes. Payloads must fit 602 bytes. The
  sequence counter uses the original signed 32-bit reset test.
- `broadcast(id,data,length)` runs `442630`. It selects current active clients
  with state word 5 and byte `18D` clear.
- `tick()` runs `4421D0`. Delay and repeat tests use signed 16-bit values.
  The active count includes records before their removal. Transport runs
  before current byte/packet counters change and before queue compaction.
- `begin()`, `append()` and `flush()` run `461040`, `460ED0` and `460FB0`.
  Begin resets the cursor. Append reports a descriptor length mismatch through
  the required diagnostic callback, then reads current flags and buffer state.
  Flush reads the current local peer at `501340`.

Bind current client spans and cells `50321C` and `501340`. Bind each reached
`4977E0` transport and `4564B0` diagnostic body. They run synchronously.
Missing reached callbacks raise errors. Prior source writes stay in place.
Transport receives mutable frame bytes and can change current state before
subsequent reads. Keep callback bindings stable while they run.

The server buffer is separate from the global message queue. Local peer
flushes append to `GameMessageStage`'s existing local echo. An append requires
`echo size + packet size < 8192`. Overflow drops the packet and still returns
its completed size. The message stage consumes that same echo through
`check_receive()` and `dispatch_received()`.

Queue payload copying uses forward four-byte chunks, then remaining bytes.
Server serialization copies forward one byte at a time. Source aliases within
these valid spans retain those access rules. Descriptor IDs must name the
loaded table. Invalid spans and source buffer overruns raise errors.

Source: `Df.exe`, SHA256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Retained source: `.local/reports/mission-logic-builders-root.json` and
`.local/reports/mission-client-outbox-source.json`.

`check_mission_client_outbox_original.py GAME_FOLDER PROBE --report FILE`
compares actual original instructions with complete client record bytes,
server buffer bytes, cursor, echo, counters and callback packets. Native
checks also join client enqueue, outbox flush and live message receipt.

Original replay: 154 cases, zero differences. Report:
`.local/reports/mission-client-outbox-original.json`. Native checks pass with
AddressSanitizer, UndefinedBehaviorSanitizer and copy elision disabled. Source,
native test and probe compile as strict Windows objects.
