# Peer timers

`mission_peer_timers` ports source `4477F0`. Bind current shared cells `6E5778` through `6E5880`. The first 16 rows have a 16-byte stride: ID, X, Y, count. The last three cells hold target X, target Y, and the movement count.

Use pure current-state views for `read` and `fp_control`. Apply `write` to the shared cells at once. Supply required terrain height (`47DF40`) and expiry (`4470C0`) handlers. These handlers run at the source call sites and can change later rows and the final movement count.

The expiry handler receives the current ID and a mutable seven-word local record: X, Y, Z, three zero words, and raw terrain height. Keep this record local to the call. The body reads the ID after the terrain handler.

The Z conversion uses the source double at `4CC6B4`, current x87 precision and rounding, `49BAE6` truncation, and a signed 32-bit store. Reserved precision and signed division overflow raise errors. The frame keeps earlier source writes when a required operation fails.

Run the source comparison:

```sh
.local/tools/python/bin/python tools/check_mission_peer_timers_original.py .local/reference-game/Df.exe build/mission_peer_timers_probe --report peer-timers-report.json
```
