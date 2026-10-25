# Mission session

Create `MissionSession` after mission initialization and client placement. Keep every context owner alive. Destroy the session before those owners.

Use the current runtime, actor state, clients, sound, scripts, input and extra-actor memory. The runtime owns `live_types`. The session owns its frame, event, message, outbox, zone, projectile-request and trail state. Projectile records remain in `MissionTickState`.

Construct the session before routing its source cells. Route `claims`, `read` and `write` before the sound fallback. Publish the selected profile mask before construction. Keep the extra-actor and definition memory routes in place.

Bind the input row provider to `mission_session_input_query_row(sound, row)`. Supply the host pump, clock, elapsed audio frames and current camera providers. Set `source_event_file` to the current BMS filename for source event tables. `install` loads the actual file into the current arena and binds current markers. Source events read the same objective mask `67DBEC`, flags and frame counter as the other services. Each fired-byte reference reads its actual wrapping arena address. Set the event policy and diagnostic for owned fixtures. Call `install`, then `step`. Read the state request after each step. Send script packets through `script_message`. Route source `42EBA0` to `end_events()` while Session is installed and all context owners are alive.

Use the current parser message `0B` for client readiness. The `446A30` route checks its current E2-byte packet, uses separate Ready and Reject caller frames, and sends through the same outbox. Keep physical actors, people, marker lists, clients, RTXT and HUD providers live. Game runs weapon startup `46BFB0` against current item definitions and publishes the nine terrain content values before Session starts. The formatter supports the exact startup and Ready literals. Bind each reached transport call. Run `native_mission_session GAME_FOLDER MISSION client-ready` for C05M07.BMS and C06M01.BMS. Keep remote admission as an explicit test input and require transport after local Ready handling.

Keep scoreboard, squad, result and peer-buffer cells under the session memory route. Periodic services use the same clients and outbox as message receive. Call `request_result(mode, caller_packet)` with current packet storage. The result returns through the shared message queue. Its row count follows the current client filter.

Pass a `MissionResultImage` as the third constructor argument when the host keeps results across states. The session uses these same bytes. Keep them alive through Session teardown and the Post/Debrief states. The two-argument constructor keeps its result storage inside Session.

Keep source x87 control in the session. Startup `490EEF` calls the CPU initializer, whose `4A1217` reset sets `037F`. Person-rate events use this value through the current control provider.

Helicopter conditions use current people, marker lists, type speeds and event flags. Keep their sixteen-byte caller frame separate from the helicopter update frame. Current actor pointers select targets by their source address and identity.

Helicopter altitude uses its own 64-byte caller frame. Ground queries use the current target list, model geometry and terrain. Publish each accepted surface to the same actor ground handle before processing the next face. Keep geometry inputs stable during publication.

Tree updates read all 256 physical player slots from the current actors and retained tail. Definition animation pointers use current cache weights through `MissionAnimationTableLive`. Reserve `20800000..20FFFFFF` for these tables. Tree insertion uses the shared primary actor owner.

Seagull updates and movement use that same primary owner, including all 1,024 physical slots. Keep the 28-byte AI frame separate from the tree frame. Bind current terrain, math, sound and shared random state. Preserve the health and flags written by the source producer.

Game routes the closed Debrief `493A80` formats through current sound memory after HUD formatting. Parameter set/get `4C4F40/4C4F60` use the current table pointer and shared return cell. Keep parameter allocation and initialization with their source owner. Game publishes current definition count `C23814`. Source `482910` clears the current definitions’ model, husk and character links; the AI model view receives the same model clear. Allow aligned zero DWORD stores to these three resource fields.

Route `leave_service` before the sound fallback for `403A50`, `4401E0`, `456E30` and `42EBE0`. The profiler call takes one index and uses the same frame profiler as mission stepping. Keep Session installed through these calls. The peer reset uses current clients, squads, result builder and outbox with its own 206-byte packet. Call `reset_events` before releasing the sound arena. Game calls it at host teardown; the source cleanup chain uses `42EBE0`.

Call `profile_leave(current_profile)` for `401E70` before Session teardown. Supply the current selected profile bytes. Restore saved rules `67DBE4` to live `67F548` at the preceding source leave site. The call uses current events, result bytes and x87 control.

Route `hud_service` before the sound service fallback. HUD history uses the shared frame timers. Campaign text uses the current RTXT table at `503238`.

Keep installed callback slots under session control. `uninstall` restores the previous world bindings and removes the callback IDs it installed. Other callback IDs remain present. Each reached foreign body needs a real provider.

Use the supplied-asset tests for Session, HUD, source-event bindings and client readiness. Keep owned event fixtures and source-arena checks separate. Use actual loader-cell inputs and record each selected mission filename. Require complete actor, message and exit checks before enabling Game frame updates. Audio devices stay closed.

The game installs the session during mission start and destroys it before its context owners. Game updates currently poll input and draw the scene. Run the asset test for mission simulation checks.
