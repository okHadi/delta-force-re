# Mission session

Create `MissionSession` after mission initialization and client placement. Keep every context owner alive. Destroy the session before those owners.

Use the current runtime, actor state, clients, sound, scripts, input and extra-actor memory. The runtime owns `live_types`. The session owns its frame, event, message, outbox, zone, projectile-request and trail state. Projectile records remain in `MissionTickState`.

Construct the session before routing its source cells. Route `claims`, `read` and `write` before the sound fallback. Construction reads the current source values. Keep the extra-actor and definition memory routes in place.

Bind the input row provider to `mission_session_input_query_row(sound, row)`. Supply the host pump, clock, elapsed audio frames and current camera providers. Set the event policy and its diagnostic explicitly. Call `install`, then `step`. Read the state request after each step. Send script packets through `script_message`.

Keep scoreboard, squad, result and peer-buffer cells under the session memory route. Periodic services use the same clients and outbox as message receive. Call `request_result(mode, caller_packet)` with current packet storage. The result returns through the shared message queue. Its row count follows the current client filter.

Route `hud_service` before the sound service fallback. HUD history uses the shared frame timers. Campaign text uses the current RTXT table at `503238`.

Keep installed callback slots under session control. `uninstall` restores the previous world bindings and removes the callback IDs it installed. Other callback IDs remain present. Each reached foreign body needs a real provider.

The stock test uses the supplied C05M07 assets and real client startup. Strict mode stops at frame 48 on event reference 74. With `missing_not_fired`, 256 frames complete and two repair diagnostics occur. The 2,048-frame run checks enemy projectiles, periodic services, result delivery and callback cleanup. Placement and cold CRT start remain explicit test inputs. Audio devices stay closed.

The game installs the session during mission start and destroys it before its context owners. Game updates currently poll input and draw the scene. Run the asset test for mission simulation checks.
