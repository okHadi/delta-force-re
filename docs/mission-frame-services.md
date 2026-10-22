# Mission frame services

Keep the supplied cells and providers alive during each call. Bind camera
position `6BA3D0/4/8` to `MissionTickState::world.world_center`. Set camera
actor `6BA3F0` from the current camera source.

Send stage calls to `MissionFrameServices::stage()`. Chain `nullopt` to the
next provider. Owned stages are `461350`, `461340`, `456E30`, `479ED0`,
`4702A0`, `42AB70` and `4562A0`.

Supply the current RDTSC value and four actor words for the profiler and
camera. Supply a synchronous output-word writer for statistic fetch/reset.
Set the shared request cell through `461340`; read it through `461350`.

Supply script counts, step delay bytes and the reached `456110`, `447C60`,
`456000` and `455B40` bodies. Providers can change script resources and
queue entries. The scheduler reads the current queue after each body.
The `456000` provider removes its completed queue entry.

Run `native_mission_frame_services` for provider and error checks. Run
`check_mission_frame_services_original.py GAME PROBE --report REPORT` for
original instruction comparisons.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
