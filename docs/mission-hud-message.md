# Mission HUD messages

Use one host memory owner for 40 records at `BD34B0`, each `0x7C` bytes.
Each record has 120 text bytes and a metadata DWORD at `+0x78`. Keep the
`BD4810` counter and existing three `MissionFrameServices` timers current.

Call `mission_hud_message_clear` at the source `479E90` startup boundary.
It clears history and the counter, then writes `FFFFFFFF` to the timers.
The source input initializer `47A300` reaches this clear before `477D00`.

Pass caller-owned `MissionHudMessageCall` storage through insertion. The body
captures its text pointer at entry and reads current metadata after copying.
Keep the object and text readable through callbacks. Insertion shifts current
DWORDs, clears the first record, shifts the shared timers, and sets timer 0
to `4D8`. It scans current text and copies at most 119 bytes. Text can alias
history; current writes then affect its scan and copy. No text pointer remains
in the queue. Timer expiry preserves history. Existing `479ED0` decrements
the shared timers with 32-bit wrap.

`mission_hud_triggered_lookup` uses current campaign RTXT pointer `503238`,
canonical `67DBC9` bit `20`, and real `49A092`. Bind `404DD0` through
`mission_hud_triggered_text` with a live 32-byte caller frame. Route its calls
to the exact `ID%03i` formatter, triggered lookup, and message insertion.
Keep the formatter's literal at `4C82FC` unchanged through the call and its
destination separate. The bounded helper accepts this literal only.

Load the campaign RTXT owner from `dfcamp%02i.bin`, using current profile
campaign `+28` plus one. The source producer releases old `503238`, checks
file presence, loads through `499F1F`, and publishes the new pointer. Keep
script text pointers current. Resolve weapon message markers through their
actual source text producer before insertion.

Original checks compare queue bytes, counters, timers, lookup results,
ordered reads, writes, and calls. Caller argument accesses map to the live
call object. Formatter checks execute actual `493A80` instructions with flat
32-bit segments and compare output and return values. Native checks join
real RTXT lookup and shared frame timers. Rendering stays with its host.
