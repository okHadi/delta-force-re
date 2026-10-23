# Helicopter update

`mission_object_helo_update` owns the complete `450E40` callback. Bind it to
the current physical actor, person, definition, globals, and local frame owners.
`bind_mission_object_helo_update` registers the normal message-zero update.
Use the direct entry for another source message.

Provide an aligned `0x44`-byte frame. Keep its initial bytes caller-owned.
Map all frame bytes through the same read/write providers. Use a separate frame
for each active call and run a registered fixed-frame callback in sequence.
Foreign pose calls receive current pointers into that frame. Target `+3C` and
sound-active `+40` remain current across callbacks.

Bind each reached nested service at its source address. These include sound
attach/stop, pose/visual, target queries, movement, ground queries, crash, clear,
and shared `477860` random calls. The callback retains its initial person
pointer. Timer publication rereads the actor delay at both source read points.
An error retains earlier writes.

Run `check_mission_object_helo_update_original.py GAME PROBE --report REPORT`
to compare complete records and frame bytes, every source read/store, message
branches, calls, and failure prefixes. Nested services use explicit fixtures
in this comparison. The native test joins real `43EF80` to the shared 1024-row
audio registration owner and current mission actor/person records.
