# Item resource loader

`mission_item_resource_load` implements `45D6F0` from the supplied `Df.exe`.

Pass the item index and a stable, aligned four-byte flag address. Keep the flag in the same current memory owner as the item table and models. The graphic loader writes this flag. Keep its value until the parent reads it.

Bind current reads and writes for these fields:

- `C23810`: item table pointer; `C23814`: signed item count. Each item uses `13C` bytes.
- `503138`: colour bank gate; `503228`: current animation script key.
- Item names at `2C`, `3C`, `4C`, `5C`; type at `28`; model, husk, CHR and ANM outputs at `6C`, `70`, `74`, `78`; effect count at `9C`.
- Model tags at `28` through `34`, writable draw callbacks at `3C` through `48`, matrix callback at `4C`, and first LOD pointer at `50`.
- First LOD words at `40` and `58` from the same loaded model.

Bind each reached service:

| Address | Service |
| --- | --- |
| `4564B0` | Report an invalid item index. Processing continues if this call returns. |
| `486530` | Load or find the graphic or husk; write the caller flag. |
| `42CBB0` | Load the named CHR resource. |
| `42D200` | Load the named ANM resource with the current script key. |
| `4818F0` | Resolve a draw callback tag. |
| `481A00` | Resolve a matrix callback tag. |

Use the current cache as the model and LOD owner. The 16 source callback rows are in `model_callback_tags`. Use row zero for an unknown tag. A nonzero item graphic ends this loader before further model reads. Type 8 gets its frame count from the signed high words of first-LOD fields `40` and `58`, with a 32-bit product.

The original checker runs `45D6F0`, `4818F0` and `481A00`. It compares all fixture bytes and the parent read, write and call order. Graphic, CHR, ANM and diagnostic calls use explicit mutable providers. The native stock test reads four models from `MissionAssetCache` and runs the actual `45DD40` producer through this loader. Its item lookup and cache-load call are explicit test providers. The production host must bind those services to its current resource owner.

Evidence: `.local/reports/mission-effect-metadata-source.json`, procedure `45D6F0..45D965`. The exact source SHA is checked by the original replay tool.
