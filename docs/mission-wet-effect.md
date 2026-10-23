# Water actor effect

`mission_wet_effect` ports `45DC70` from the supplied `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Map one caller frame of at least `0x130` bytes. The first `0x114` bytes hold the temporary actor. Current arguments are at `+120` (effect index), `+124` (X), `+128` (Y), and `+12C` (Z). Share this memory with synchronous callbacks.

Bind current reads, stores, and these required bodies:

- `49A620(frame,0,0x114)` clears the temporary actor.
- `477860()` uses the shared mission random state, twice.
- `45D970(frame,1)` inserts through the canonical extra actor owner.

The body captures the index from `8E44B0` and the definition pointer from `C23810` before the clear. It reads coordinates after the clear. The first random result sets yaw; the second sets scale. It reads the captured definition's current `+84` and `+90` fields after both random calls. Arithmetic wraps at 32 bits. Each missing provider reports an error at its reached source point.

`45E0F0` calls this body with `{2,cachedX,cachedY,initialWater}`. Bind its sound call separately. The temporary actor enters the existing `822E80` pool. `485290` passes all 1024 physical slots to `485040` at `4852F5`; the renderer uses `mission_scene_transient_records` and the current actor image. This collection precedes the near queue and water passes. Use this existing render path for the water actor.

Retain source evidence in `.local/reports/mission-wet-visual-45dc70-source.json`. The original checker compares the full temporary actor, memory read/store order, callback mutations, wrapped values, and failure prefixes. The native join uses the real shared random helper, canonical actor insertion, and current scene records. Bind the real definitions and reached callbacks for each live mission.
