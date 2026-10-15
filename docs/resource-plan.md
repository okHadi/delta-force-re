# Mission resource loading

`build_resource_plan` follows single-player startup at `402EC0`.

1. Load the soldier pair selected by mission theme and night flag.
2. Load both passes of the 39 weapon records, then the three fixed weapons.
3. Load the effect items.
4. Load enabled mission groups 0, 1, and 3 in file order.

Names compare without letter case. The first successful model load fixes its
registry position and palette bank. Banks are vehicle 1, decoration 2,
person 3, building 4, object 5, and other types 0 in 32-bit mode.
Group 2 markers have no startup model pass.

`MissionAssetCache::load_runtime` takes an empty cache and loads this plan.
It returns loaded model names and banks in registry order. Missing model
files produce null handles, as in `486530`; their names appear in
`missing_models`. The stock weapon table requests missing `null.3di`.
Character and animation files use separate caches.

The plan records squad and powerup slot removals. Apply these removals in
the actor runtime. Supply `ResourcePlanOptions` for the caller's mode flags.

Verification: `native_resource_plan`, `resource_plan_probe`, and
`check_resource_plan_original.py`. The check covers 62 missions, 21,307
item calls, and 6,064 file requests. It compares a separate plan made from
stock files and source constants. Original x86 replay checks 20 bank cases
and the person model/character/animation load order.
