# Model memory codec

`MissionModelMemory` maps typed model handles to separate raw graphic, LOD and hardpoint addresses. Keep one codec for the current resource owner. Keep its cache and providers alive through all users.

Pass a read-only current-model provider, a same-file byte provider and explicit bindings. The constructor validates the decoded file identity, tags, LOD headers and hardpoints against the cache. Each binding needs one callback seed phase:

- `pre_producer()`: take the five callback words from source header `3C..4F`, before the real first-load producer.
- `post_initialized(words)`: take the five canonical words supplied by a proved callback-init phase.

Both phases read the same file for validation. The post-init phase uses the supplied callback words. Keep the actual graphic loader in charge of its first-load flag.

Use `source(handle)` and `handle(raw)` at typed boundaries. Both preserve zero. Raw pointers must name exact current graphic bases. Translate current item model/husk fields `6C/70` and actor graphic fields `10/14` at their memory boundary. Keep byte reads of each pointer field tied to the same encoded current value.

| Address range | Current owner |
| --- | --- |
| `20000000..200FFFFF` | Graphic slots, stride `80`, source record extent `7C`, cap 500 |
| `20100000..201FFFFF` | LOD slots, stride `100`, source record extent `B8`, total cap 601 |
| `20200000..207FFFFF` | Exact hardpoint spans with separate alignment gaps |

The codec reads these proved fields:

- Graphic `14`: LOD count; `28..37`: four current render tags.
- Graphic `3C..4F`: five writable callback words.
- Graphic `50..5F`: four stable current LOD pointers.
- LOD `40..43` and `58..5B`: current raw header words.
- LOD `A0..A7`: current hardpoint count and stable hardpoint pointer.
- Hardpoints: current signed 24.8 components, stride `0C`.

Reads use widths 1, 2 or 4. Callback writes support byte aliases across adjacent callback words. Other writes require their actual field owner. `claims` and `record_base` reject gaps, unknown fields, partial overlaps and invalid widths. Empty hardpoint spans have a nonzero pointer and zero valid bytes. Keep LOD count, header size and hardpoint count stable for the binding lifetime. Current field and hardpoint values come from the cache on each read.

Use the exact `model_callback_tags` rows for source callback IDs. Keep these IDs separate from callable native pointers. Bind real `486530`, `42CBB0` and `42D200` services when reached. The codec supplies memory views; the loader owns resource lookup, file reads, allocation, first-load flags and failure state.

The native test uses the supplied Gazelle and `bexp` files with one cache. It runs both helicopter pose callers with canonical actor handles and runs `45D6F0` through a typed item boundary. Its cache-load call is an explicit test provider. The original checker runs those callers and the real matrix and callback lookup bodies. Its initial runtime pointer fixups are explicit fixtures based on the source loader contract. These tests do not replace the required production loaders.

Source evidence: `.local/reports/mission-model-codec-plan.md`, `mission-model-codec-0x486370-source.json`, `mission-model-codec-0x486530-source.json`, and `mission-model-codec-0x486040-source.json` (actual entry `485E30`).
