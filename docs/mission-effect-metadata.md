# Effect model metadata

Use the seven-argument `assemble_mission_runtime` overload for asset-backed missions. Supply the current sound resolver and a model resolver that returns `cache.model(name)` from the cache that loaded the resource plan. Keep the cache alive through assembly. `initialize_mission` supplies this model resolver.

For each loaded type-8 graphic, assembly reads the first loaded LOD header. Item `+9C` receives `(signed header[58h] >> 16) * (signed header[40h] >> 16)`, with 32-bit product bits. A null graphic or first LOD preserves the count. A present LOD has a complete header.

The same runtime definition supplies the canonical mission item definition `+9C` when `init_mission_tick` runs. Keep model mesh and callback metadata in the model resource owner.

Run `native_mission_effect_metadata` for prepared-data checks. Add `GAME_FOLDER` to check supplied `C06M01.BMS` resources and the canonical mission definitions. Run `check_mission_effect_metadata_original.py GAME_FOLDER PROBE --report FILE` for the original `45D6F0` comparison. Its file loader returns the supplied model and a zero first-load flag; the check covers item metadata after that load.
