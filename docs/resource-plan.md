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

Call `cache.load_projectile_runtime(resources)` after `load_runtime()` and
before `assemble_mission_runtime()`. This adds the separate `4633F0` pass:
item IDs `FA1`, `FC1`, `FCA`, in that order. The pass uses the same item,
model, character and animation registries and the saved palette-bank mode.
Each appended call has stage `projectiles`. The first model load fixes its
handle and bank. Repeated passes keep the existing file bindings.

`build_resource_plan()` supplies `402EC0`. Use
`append_projectile_resource_plan(plan,input)` for the later item-load pass.
The original single-player caller runs these bodies at `403C12` and
`403C17`. Supply loaded projectile models before a graphic spawn branch.
Missing files keep null model handles.

The plan records squad and powerup slot removals. Apply these removals in
the actor runtime. Supply `ResourcePlanOptions` for the caller's mode flags.

Verification: `native_resource_plan`, `resource_plan_probe`, and
`check_resource_plan_original.py`. The check covers 62 missions, 21,307
item calls, and 6,064 file requests. It compares a separate plan made from
stock files and source constants. Original x86 replay checks 20 bank cases
and the person model/character/animation load order.

Run `native_resources` for cache, shared-name, missing-file and bank checks.
Run `check_resources_original.py GAME RESOURCES_PROBE --report REPORT`
for the original `4633F0`, item lookup and loader bodies with the stock
model registry and assembled handles.
