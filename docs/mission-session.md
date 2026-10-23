# Mission session

Create `MissionSession` after mission initialization and client placement. Keep every context owner alive. Destroy the session before those owners.

Use the current runtime, actor state, clients, sound, scripts, input and extra-actor memory. The runtime owns `live_types`. The session owns its frame, event, message, outbox, zone, projectile-request and trail state. Projectile records remain in `MissionTickState`.

Construct the session before routing its source cells. Route `claims`, `read` and `write` before the sound fallback. Construction reads the current source values. Keep the extra-actor and definition memory routes in place.

Bind the input row provider to `mission_session_input_query_row(sound, row)`. Supply the host pump, clock, elapsed audio frames and current camera providers. Set the event policy and its diagnostic explicitly. Call `install`, then `step`. Read the state request after each step. Send script packets through `script_message`. Route source `42EBA0` to `end_events()` while Session is installed and all context owners are alive.

Keep scoreboard, squad, result and peer-buffer cells under the session memory route. Periodic services use the same clients and outbox as message receive. Call `request_result(mode, caller_packet)` with current packet storage. The result returns through the shared message queue. Its row count follows the current client filter.

Pass a `MissionResultImage` as the third constructor argument when the host keeps results across states. The session uses these same bytes. Keep them alive through Session teardown and the Post/Debrief states. The two-argument constructor keeps its result storage inside Session.

Keep source x87 control in the session. Startup `490EEF` calls the CPU initializer, whose `4A1217` reset sets `037F`. Person-rate events use this value through the current control provider.

Helicopter conditions use current people, marker lists, type speeds and event flags. Keep their sixteen-byte caller frame separate from the helicopter update frame. Current actor pointers select targets by their source address and identity.

Helicopter altitude uses its own 64-byte caller frame. Ground queries use the current target list, model geometry and terrain. Publish each accepted surface to the same actor ground handle before processing the next face. Keep geometry inputs stable during publication.

Tree updates read all 256 physical player slots from the current actors and retained tail. Definition animation pointers use current cache weights through `MissionAnimationTableLive`. Reserve `20800000..20FFFFFF` for these tables. Tree insertion uses the shared primary actor owner.

Seagull updates and movement use that same primary owner, including all 1,024 physical slots. Keep the 28-byte AI frame separate from the tree frame. Bind current terrain, math, sound and shared random state. Preserve the health and flags written by the source producer.

Game routes the closed Debrief `493A80` formats through current sound memory after HUD formatting. Parameter set/get `4C4F40/4C4F60` use the current table pointer and shared return cell. Keep parameter allocation and initialization with their source owner.

Route `leave_service` before the sound fallback for `403A50` and `4401E0`. Keep Session installed through these calls. The reset uses current clients, squads, result builder and outbox with its own 206-byte packet.

Route `hud_service` before the sound service fallback. HUD history uses the shared frame timers. Campaign text uses the current RTXT table at `503238`.

Keep installed callback slots under session control. `uninstall` restores the previous world bindings and removes the callback IDs it installed. Other callback IDs remain present. Each reached foreign body needs a real provider.

The stock test uses the supplied C05M07 assets and real client startup. Strict mode stops at frame 48 on event reference 74. With `missing_not_fired`, 256 frames complete and two repair diagnostics occur. The 2,048-frame run checks enemy projectiles, periodic services, result delivery and callback cleanup. Use the explicit end-event fixture to check the current result publisher while actors, clients and messages are live. Placement and cold CRT start remain explicit test inputs. Audio devices stay closed.

The game installs the session during mission start and destroys it before its context owners. Game updates currently poll input and draw the scene. Run the asset test for mission simulation checks.
