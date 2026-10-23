# Mission logic

Use `mission_logic_frame(bindings)` for the complete `443C50` stage. Keep each cell, client record, and actor record with its current owner. Supply current reads and writes through the bindings.

The frame runs the peer receive drain, `4477F0`, periodic stages, client maintenance, round countdown, network throttle, time update, client processing, actor replication, `444F60`, and the distinct `4421D0` client outbox drain in source order. Each reached foreign body is required and runs before the next read.

The event counter `6E4734` uses signed comparison. Other periodic counters use unsigned comparison. Route `42EAF0` to `EventStage::tick()` and keep `6E4734` with the frame cells. The outer mission loop owns `46F5C0`.

Bind these shared cells: `67F520`, `67F528`, `67DBC0`, `67F534`, `6E558C`, `67DBBC`, `67F538`, `67DBE8`, `6E4730..6E475C`, and actor counts `8CFA18/1C`. Bind 17 current client records and current group-three and group-zero actor fields. `client_actor_flags` reports the current client's actor presence and `+6C` byte.

`call(441920, {slot})` uses a normalized client slot. `call(447FB0, {slot})` uses the original client index. Other reached bodies take no arguments. The host executes the real body and returns its result. The `460E40` result controls the receive drain.

Send reliable packets through the real `442630` producer. Replication includes message `0E`, `29`, `34..38`, `3B`, and `4F`. Supply three explicit padding bytes for `34` and `37`. Actor dirty flags are read again after delivery and cleared through the current actor owner.

`MissionLogic::inspection_step()` provides an event/result inspection fixture. Its local bus and supplied result rows support inspection tests. Live result handling uses `mission_logic_request_result`, `mission_logic_build_result`, and `mission_logic_receive_result`.

Run `native_mission_logic` for required providers, counter edges, current callback state, packets, and inspection checks. Run `check_mission_logic_original.py GAME PROBE --report FILE` for the original full frame, all 17 client images, actor bytes, and ordered calls. Foreign bodies are explicit test providers in that comparison.

Use `MissionLogicResultBindings` for live results. `444CA0` runs `446100(0)` before its end guard, builds a current result packet for each active client, publishes through that client’s real `441750` outbox, and sets the end cell after publication. `4403C0` collects current clients, calls `448130` in source sort order, and reads current statistics after the sort. The caller supplies the initial 206-byte packet; untouched bytes retain their values.

Bind the real reached bodies `446100`, `448D20`, `446DA0`, and `448130` through `state.call`. Comparator arguments use client slots. Bind `publish(client, 1E, packet)` to the current client outbox producer. The `4421D0` frame stage owns later delivery. Each provider completes its state changes before the next read.

Route the message stage’s synchronous `471010` call to `mission_logic_receive_result`. Supply a nonnull 206-byte payload. Bind current local actor presence, group-three actor identity, original actor addresses, and actor `+6E` fields. Actor slot zero is valid. `actor_name` reads the current person name or runs the real `404CF0` fallback. `store_name` writes the name and its end byte to the result buffer. Signed words and signed ratio faults retain source behavior. Bind `43E720(499)` to the muted audio scheduler.

Keep result cells `67F714..67F720`, result rows at `67F724 + row*0x24`, and statistics `67FBEC..67FC24` with their current owner. Bind result request cells `67DBC0`, `67F520`, `67F528`, `67F534`, `6E558C`, `67F52C`, `67F53C`, `6E4758`, `6E576C/70/74`, and squad records at `6E80C8 + squad*0x68`. Providers use byte offsets and hexadecimal addresses.

Run `check_mission_logic_results_original.py GAME PROBE --report FILE` for the three original result bodies. The check compares complete packets, client images, result buffers, ordered calls, current names, changes made by providers, and signed division faults. Foreign body providers are explicit fixtures.

Run `native_mission_peer_live` for two single-player frames through the real
receive, timer, budget, round-clear, and client outbox bodies. This check uses
shared current cells, inactive client inputs, and an empty peer buffer. Packet
parsing and timer expiry require their reached handlers.

The same check sends a reliable shot from a client without an actor through
the shared peer buffer and real parser. The source actor guard skips its shot
handler. The real ACK outbox and local receiver cancel the original producer
record. Keep ACK history, client records, and the producer queue with their
current owners.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
