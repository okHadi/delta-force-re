# Object callback bodies

Use `object_live_callbacks.hpp/.cpp` for429BC0,429EA0,4519A0 and451CB0 with its451850 helper. Keep required foreign services bound at their source call points.

The callbacks use0x114-byte actor records,0xB4-byte person records and the shared477860 RNG state. Use synchronous call providers for live state. Use ordered input records for isolated instruction comparisons.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Retain the exact body evidence in `.local/reports/callbacks-actor-behaviors-20261016.json`. The451850 recovery is in `.local/client-init-recovery/searchlight-run.log`.

Run the live checkers for vehicle AI, vehicle movement, searchlight AI and searchlight movement. Run native tests with AddressSanitizer, UndefinedBehaviorSanitizer and constructor copy elision disabled. Compile the source, adapters, tests and probes for Windows.
