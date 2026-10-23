# Projectile trail frames

Keep one `MissionProjectileTrailCells` owner for insertion, clear, frame update, and drawing. Keep one `MissionTrailFrameClock` owner for `A158A0` across mission changes. Bind current `6E4724`, `6E4718`, and prepared camera lvalues.

Call `mission_trail_frame` after companion water and the effect queues. Original `485610` calls it at `485731`. The `4634D0` body scans all 2048 records. It stores the current clock first, clamps negative elapsed time to zero, and uses zero elapsed time when `6E4718` is nonzero. Clock and lifetime arithmetic wraps at 32 bits. Positive lifetimes subtract elapsed time. Values below four clear to zero; other values supply alpha from `(lifetime >> 2) << 3` as a byte.

The body transforms both current endpoints with `45CDA0`, clips them with `44AF80` and `44AEE0`, then submits the reached `A00` line synchronously. Color is read after projection. A callback can change later records and current camera fields. Reached transform and draw providers are required.

Use `mission_trail_draw32` for the `462DF0` line branch at `462520`. It clips screen endpoints, interpolates depth, tests against the current 16-bit depth plane, and blends all four color bytes with source signed-word arithmetic. It leaves depth unchanged. Supply a valid 32-bit color target and clip rectangle. The 8-bit lookup-table branch requires a separate backend.

Source dossiers are retained as `.local/reports/mission-trail-frame-*-source.json`. The original checker covers endpoint clipping, full frustum clipping, frame reads and callbacks, and actual 32-bit line color/depth buffers. It also runs full original frame-to-pixel cases. Callback tests use an explicit draw boundary fixture.
