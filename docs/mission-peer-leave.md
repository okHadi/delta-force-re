# Peer leave services

Use `mission_peer_reset` for source `4401E0`. Supply current memory and required calls with `MissionPeriodicBindings`. Map one caller-owned 206-byte result packet at the frame address. Keep this packet, all 17 physical clients and all 64 squads current through every callback.

The reset clears the packet, writes its current mode byte, then visits each client and squad in source order. Active client states three and four call required `4403C0(frame,client)` and `441750(client,1E,frame,CE)` before the reset rereads current client state. State one is retained. The exact source fields are cleared with their original widths. Inactive clients and squads use required `49A620`. Other fields retain their current values.

Map the bytes of one real `MissionLogicResultPacket` to the caller frame. Route `4403C0` to `mission_logic_build_result` and `441750` to the shared client outbox. Keep builder, enqueue and raw frame accesses on those same bytes. A failed child or memory provider leaves all earlier writes committed.

Use `mission_peer_leave_drain` for `403A50` and `447F60`. The initial `67F528` and `67F520` gates control entry. Each nonzero `460E40` receive result reads current `6E5588` and calls `442AE0` when that value is nonzero. The wrapping count increases after that branch. The first zero receive result calls `4421D0`, then `490880`. Supply the real receive, parser, outbox and host message pump. Each reached missing service throws.

Run `native_mission_peer_leave` for real canonical clients, result builder, squad collector, outbox, peer receive and parser joins. The strict receive test preserves its parsed prefix when the required transport is absent. A separate current-callback mutation clears networking after parsing to test the final drain calls. The pump is an explicit test observation. These checks do not establish network delivery.

Run `check_mission_peer_leave_original.py GAME PROBE --report FILE` for all full client/squad/frame images, exact ordered accesses, current callback changes, short-circuit gates and child/read/write failures. Its foreign calls use declared fixtures. Original stack frame addresses are mapped to the caller frame for comparison.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
