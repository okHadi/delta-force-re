# Mission script runtime

Use `MissionScriptRuntime::call` as the script provider for
`MissionFrameServices`. Keep both modules, their shared cells, and the mutable
binding owner alive. Use the same `MissionScriptQueueCells` for both modules.
Keep up to sixteen active queue entries in source order.

The runtime implements step playback `456110`, host message `447C60`, queue
removal `456000` with `455FA0` ordering, and voice cleanup `455B40`.
Use current resource bytes, words, names, text data, and audio-cache records.
Keep resource and cache addresses stable during callbacks.

Supply the step callback and its three playback arguments. Playback records
publish after the audio provider returns. Text follows playback. Cleanup
keeps shared samples until their final registered voice ends, clears the
loaded flag before release, and clears the allocation after release returns.
Use muted audio providers that preserve handle and voice state.

Run `native_mission_script_runtime` for callback and error checks. Run
`check_mission_script_runtime_original.py GAME PROBE --report FILE` for
original instruction comparisons of step calls, packets, removal, and cleanup.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
