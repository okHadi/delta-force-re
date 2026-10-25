# Event marker binding

`mission_event_markers_triggers` runs `42EEA0`. `mission_event_markers_actions` runs `42F020`. Bind both calls after the `42EC90` event pointer fixup.

Supply current scalar read and write functions in `MissionEventResourceBindings`. Keep the event allocations, physical marker records, and item definitions in the same live memory graph. Reads use widths 1 and 4. Writes use width 4. Provider errors retain completed writes.

The functions capture event base `4F2BCC`, then read signed count `4F2BC8`. Each event occupies 24 bytes. Run counts are BYTE fields `+15` and `+16`; run pointers at `+4` and `+8` select 32-byte trigger and action records. The functions reread current counts and pointers at the original access points.

Markers use physical group 2 at `8ACE80`, stride `114`, and current count `8CF900`. The current definition pointer at marker `+4` supplies type `177D` at definition `+20`. Marker `+7C` supplies the DWORD binding ID. Positions at `+24` and `+28` use signed comparisons. The host maps definition pointers to the same current item owner.

Trigger kind 1, subtype 10 binds ID `+10`. Its rectangle occupies `+10`, `+14`, `+18`, `+1C`. Action kinds 12 and 13 bind ID `+C`. Their rectangle occupies `+C`, `+14`, `+18`, `+1C`. The first match writes X/Y/X/Y. Later matches extend the bounds. An unmatched trigger clears kind and subtype; an unmatched action clears kind.

The original comparison covers both complete bodies, ordered reads and writes, current pointer and count changes, signed boundaries, and failed access state. The native asset test loads C05M07 and C06M01 through the current file and source arena, converts their actual marker records with `AiActorFactory`, and uses one canonical `MissionTickState` marker owner. `MissionEventMemory` reads the bound tables before reset and reload. Definition addresses use an explicit codec. Test file access opens no audio output device.
