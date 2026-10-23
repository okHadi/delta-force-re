# Event actor flags and removal

Use `mission_event_actor_flags` for 45F4C0. Pass the shared A158BC damage cell by reference. Use `mission_event_actor_remove` for 45F5B0.

Both actions return immediately for side zero. They scan groups 1, 3, then 0. The side is the signed actor word at +72. Inactive actors participate. Each group count is read at entry and after every candidate. Keep current actors and count bindings alive through synchronous callbacks. Negative entry counts skip that group. Reached storage outside the canonical tables raises `MissionTickError`.

The flag action stores word +74 as FFCE, clears A158BC, then reads current +110 and calls that body with message 1. Bind `update` to the actual reached body. A missing provider or null callback raises an error after the two source stores. Store observers run before each store. Read observers run before each outer source read.

The removal action calls the included 4471D0 helper. A zero 67F528 host cell returns before removal. Otherwise, the helper captures the unsigned raw actor offset from 7CCA80 divided by 114. It calls 45DA30, then broadcasts type 10 with the low two index bytes. Group 0 and group 1 follow the same source arithmetic.

Use `mission_event_actor_flag_bindings` with the current `MissionActorLifecycle` and `MissionClientOutbox` owners. The factory binds real person/character/audio release and per-client message delivery. Supply the current actor update body separately. Callback changes to actors, counts, host state, and A158BC remain visible.

The original comparison runs 45F4C0, 45F5B0, and 4471D0 from Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. It checks ordered reads and stores, callback parameters, current count changes, signed sides, inactive rows, health-word width, and removal packet bytes. It also checks the original null-callback fault after the source stores. Native integration uses real actor lifecycle and client outbox owners with sanitizer checks. Windows object compile checks use strict warnings.
