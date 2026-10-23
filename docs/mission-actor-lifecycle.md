# Actor removal

Use `MissionActorLifecycle` for live actor removal. Keep the current
`MissionTickState`, its one `MissionAnimations` owner and the shared 1024
sound registrations alive. Links and registration actor fields use
`mission_actor_source_address(group, slot)` addresses. Linked records must fit
the current table and its physical capacity: 1024, 1024, 512, or 256 rows for
groups 0 through 3.

- `remove(ref)` runs `45DA30`. A null reference returns. A class-3 actor first
  drops its link and releases its current CHR instance. The current person
  record and its extra fields are cleared. The actor and actor-backed runtime
  fields are cleared. Its driver and controller reference are detached. The
  external step cache at `C91E90` stays in place.
  Matching sound registrations are then cleared.
- `drop_link(ref)` runs `4604C0`. The actor link is cleared before the linked
  flags change. Bind sine/cosine `4776C0` and terrain height `47DF40` for each
  reached call. Callbacks run synchronously. Position reads after callbacks
  use current actor state; additions wrap at 32 bits.
- `release_character(handle)` runs the `42CD30`/`439C40` release through the
  existing animation owner. Zero returns. A nonzero handle must name a current
  instance. Its controller and action storage are released.
- `unregister_audio(ref)` runs `43F070` on the shared registrations. It clears
  each matching 16-byte record. The sound scheduler owns backend playback.

Missing reached providers raise errors. Earlier source effects remain in
place when a callback raises an error. Use the same mutable actor and person
pools for callbacks and removal.

Source: `Df.exe`, SHA256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
REA source and assembly: `.local/reports/mission-actor-lifecycle-source.json`.

`check_mission_actor_lifecycle_original.py GAME_FOLDER PROBE --report FILE`
runs the real removal, link-drop, CHR wrapper/release and registration cleanup
instructions. The heap free uses a fixture; live storage uses C++ ownership.
The comparison checks actor/person bytes, instance availability, registrations
and foreign call arguments. Native checks cover callback changes to current
person/CHR bindings, live drivers and missing-provider error order.

The original comparison covers 432 cases. The report is
`.local/reports/mission-actor-lifecycle-original-final.json`. Native lifecycle
and animation release checks pass with AddressSanitizer,
UndefinedBehaviorSanitizer and copy elision disabled. The lifecycle source,
animation source, both native tests and probe compile as strict Windows
objects. These checks use memory and open no audio device.
