# Event runtime

Source: `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Load BMS tables with `EventStage::load`. This runs both marker binders and
changes the loader flag. `load_tables` supplies tables for isolated callers;
run the binders and set the loader flag for that path.

Keep event, trigger, and action tables stable during host calls. Update record
fields in place. The scheduler reads current action and trigger bases, counts,
flags, and timers after each call. Host errors stop the current pass and keep
writes already made. Bind every reached `EventHost` method. Missing bindings raise `EventError`. Use valid table and flag indexes.

Event words use this layout:

| Word | Value |
| --- | --- |
| 0 | Repeat bit 0; bit-pass flags 1 and 2 |
| 1 | Trigger run index |
| 2 | Action run index |
| 3 | Repeat countdown low16; reload high16 |
| 4 | Delay countdown low16; reload high16 |
| 5 | Fired byte; trigger count byte; action count byte |

Timers subtract `0x80` with signed 16-bit tests. A zero repeat reload clears
the fired byte and keeps the current countdown. Action kinds 5, 14, and 15
set bits in `67DBEC`, `6E0254`, and `6E0250`. Reset keeps the event cursor,
frame counter, and `67DBEC`.

Use one owner for frame counter `6E4734`. With `mission_logic_frame`, keep
that counter in its host and route `42EAF0` to `EventStage::tick`. Use
`frame_driver_step` when the event stage owns the counter. The signed counter test calls
one tick above 15 and resets the counter. Each tick processes one of eight event buckets.
Route `42EB60` and `42EBA0` to the two bit-pass methods.

Supply these current host reads:

| Method | Source data |
| --- | --- |
| `object_index_dword` | `7936A4`, `7936A8`, or `7936AC`, with stride `0x40` |
| `probe_460330` | Current trigger record, actor identifiers, and rectangle bounds |
| `probe_460560` | Original `460560(word3, word4)` result |
| `world_67f520_is_zero` | Current `67F520 == 0` |
| `world_67f549_bit1` | Current `67F549 & 2` |
| `external_event_fired` | Legacy fired byte outside the loaded event payload |

Object identifiers come from actor `+70` high16, filled from BMS byte `48`.
The two count cells start from the loaded actors. Keep their runtime values
with the actor lifecycle. `7936A4` requires its current producer state.
Rectangle trigger `460330` reads runtime groups 3 and 0 (`7CCA80`, `7DDE80`).
Marker binding uses BMS object group index 2 and type `177D`.

Execute each reached action body synchronously:

| Kind | Host call |
| --- | --- |
| 1 | `45F670(word3, word4)` |
| 2 | `45F4C0(word3)` |
| 3 | `4601A0(action_index)` |
| 4 | `45F5B0(word3)` |
| 6 | `404DD0(word3)` |
| 7 | `43F500(word3)` |
| 8, 9, 10 | `444CA0(1)`, `444CA0(2)`, `444CA0(0)` |
| 11 | `460640(word3, word4)` |
| 12, 13 | `460280(action_index)` |

`45F670` reads runtime group 3. `45F4C0`, `45F5B0`, and `4601A0` read runtime
group 1 (`867E80`). Record-index methods read the current stage record. Bind
these bodies to their game owners. The event stage owns scheduling and routing.

Run the silent original comparison:

```sh
python tools/check_events_original.py Df.exe EVENTS_PROBE --game-folder GAME_FOLDER
```

The check compares event, trigger, and action words, flag arrays, cursor,
frame counter, and ordered host calls. It includes 62 stock missions,
foreign-call mutations, and signed counter limits. Host effects are fixtures.
The checker executes allocator `497F60` with its 16-byte headers and alignment.

C05M07 trigger 108 reads event index 74 with 74 loaded events. Bind its external
byte explicitly. Event 66 uses that condition only to request dialogue 706;
its fired byte has no event readers. The check proves two valid heap layouts:
a fresh heap reads trigger 0's kind byte; an occupied heap reads another live
allocation. Keep the heap owner or a defined compatibility rule with the host.

Evidence in `.local/reports`: `startup-bms-events-load.json`,
`startup-bms-event-postload-a.json`, `startup-bms-event-postload-b.json`,
`startup-bms-record-convert.json`, `events-runtime-disassembly-review.json`,
`events-allocation-disassembly-review.json`, and
`events-c05m07-dependency-review.json`.
